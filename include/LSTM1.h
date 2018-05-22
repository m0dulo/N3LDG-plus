#ifndef LSTM1
#define LSTM1

/*
*  LSTM1.h:
*  LSTM variation 1
*
*  Created on: June 13, 2017
*      Author: mszhang
*/

#include "MyLib.h"
#include "Node.h"
#include "BiOP.h"
#include "AtomicOP.h"
#include "Graph.h"
#include "PMultiOP.h"
#include "PAddOP.h"
#include "BucketOP.h"

struct LSTM1Params {
    UniParams input_hidden;
    UniParams input_input;
    UniParams output_hidden;
    UniParams output_input;
    UniParams forget_hidden;
    UniParams forget_input;
    UniParams cell_hidden;
    UniParams cell_input;

    LSTM1Params() {
    }

    void exportAdaParams(ModelUpdate& ada) {
        input_hidden.exportAdaParams(ada);
        input_input.exportAdaParams(ada);
        output_hidden.exportAdaParams(ada);
        output_input.exportAdaParams(ada);
        forget_hidden.exportAdaParams(ada);
        forget_input.exportAdaParams(ada);
        cell_hidden.exportAdaParams(ada);
        cell_input.exportAdaParams(ada);
    }

    void initial(int nOSize, int nISize) {
        input_hidden.initial(nOSize, nOSize, false);
        input_input.initial(nOSize, nOSize, true);
        output_hidden.initial(nOSize, nOSize, false);
        output_input.initial(nOSize, nOSize, true);
        forget_hidden.initial(nOSize, nOSize, false);
        forget_input.initial(nOSize, nOSize, true);
        cell_hidden.initial(nOSize, nOSize, false);
        cell_input.initial(nOSize, nOSize, true);
    }

    int inDim() {
        return input_input.W.inDim();
    }

    int outDim() {
        return input_input.W.outDim();
    }

    void save(std::ofstream &os) const {
        input_hidden.save(os);
        input_input.save(os);
        output_hidden.save(os);
        output_input.save(os);
        forget_hidden.save(os);
        forget_input.save(os);
        cell_hidden.save(os);
        cell_input.save(os);

    }

    void load(std::ifstream &is) {
        input_hidden.load(is);
        input_input.load(is);
        output_hidden.load(is);
        output_input.load(is);
        forget_hidden.load(is);
        forget_input.load(is);
        cell_hidden.load(is);
        cell_input.load(is);
    }

};

// standard LSTM1 using tanh as activation function
// other conditions are not implemented unless they are clear
class LSTM1Builder {
public:
    int _nSize;
    int _inDim;
    int _outDim;

    vector<LinearNode> _inputgates_hidden;
    vector<LinearNode> _inputgates_input;
    vector<PAddNode> _inputgates_add;
    vector<SigmoidNode> _inputgates;

    vector<LinearNode> _forgetgates_hidden;
    vector<LinearNode> _forgetgates_input;
    vector<PAddNode> _forgetgates_add;
    vector<SigmoidNode> _forgetgates;

    vector<LinearNode> _halfcells_hidden;
    vector<LinearNode> _halfcells_input;
    vector<PAddNode> _halfcells_add;
    vector<TanhNode> _halfcells;

    vector<LinearNode> _outputgates_hidden;
    vector<LinearNode> _outputgates_input;
    vector<PAddNode> _outputgates_add;
    vector<SigmoidNode> _outputgates;

    vector<PMultiNode> _inputfilters;
    vector<PMultiNode> _forgetfilters;

    vector<PAddNode> _cells;

    vector<TanhNode> _halfhiddens;
    vector<PMultiNode> _hiddens;  // intermediate result without dropout

    BucketNode _bucket;

    LSTM1Params* _param;

    bool _left2right;

public:
    LSTM1Builder() {
        clear();
    }

    ~LSTM1Builder() {
        clear();
    }

public:
    void init(LSTM1Params* paramInit, dtype dropout, bool left2right = true) {
        _param = paramInit;
        _inDim = _param->input_input.W.inDim();
        _outDim = _param->input_input.W.outDim();
        int maxsize = _inputgates_hidden.size();

        for (int idx = 0; idx < maxsize; idx++) {
            _inputgates_input.at(idx).setParam(&_param->input_input);
            _outputgates_input.at(idx).setParam(&_param->output_input);
            _forgetgates_input.at(idx).setParam(&_param->forget_input);
            _halfcells_input.at(idx).setParam(&_param->cell_input);

            _inputgates_hidden.at(idx).setParam(&_param->input_hidden);
            _outputgates_hidden.at(idx).setParam(&_param->output_hidden);
            _forgetgates_hidden.at(idx).setParam(&_param->forget_hidden);
            _halfcells_hidden.at(idx).setParam(&_param->cell_hidden);
        }
        _left2right = left2right;

        for (int idx = 0; idx < maxsize; idx++) {
            _inputgates_hidden.at(idx).init(_outDim, -1);
            _inputgates_input.at(idx).init(_outDim, -1);
            _forgetgates_hidden.at(idx).init(_outDim, -1);
            _forgetgates_input.at(idx).init(_outDim, -1);
            _halfcells_hidden.at(idx).init(_outDim, -1);
            _halfcells_input.at(idx).init(_outDim, -1);
            _outputgates_hidden.at(idx).init(_outDim, -1);
            _outputgates_input.at(idx).init(_outDim, -1);

            _inputfilters.at(idx).init(_outDim, -1);
            _forgetfilters.at(idx).init(_outDim, -1);
            _cells.at(idx).init(_outDim, -1);

            _halfhiddens.at(idx).init(_outDim, -1);
            _hiddens.at(idx).init(_outDim, dropout);

            _inputgates_add.at(idx).init(_outDim, -1);
            _inputgates.at(idx).init(_outDim, -1);
            _forgetgates_add.at(idx).init(_outDim, -1);
            _forgetgates.at(idx).init(_outDim, -1);
            _halfcells_add.at(idx).init(_outDim, -1);
            _halfcells.at(idx).init(_outDim, -1);
            _outputgates_add.at(idx).init(_outDim, -1);
            _outputgates.at(idx).init(_outDim, -1);
        }
        _bucket.init(_outDim, -1);

    }

    void resize(int maxsize) {
        _inputgates_hidden.resize(maxsize);
        _inputgates_input.resize(maxsize);
        _inputgates_add.resize(maxsize);
        _inputgates.resize(maxsize);
        _forgetgates_hidden.resize(maxsize);
        _forgetgates_input.resize(maxsize);
        _forgetgates_add.resize(maxsize);
        _forgetgates.resize(maxsize);
        _halfcells_hidden.resize(maxsize);
        _halfcells_input.resize(maxsize);
        _halfcells_add.resize(maxsize);
        _halfcells.resize(maxsize);
        _outputgates_hidden.resize(maxsize);
        _outputgates_input.resize(maxsize);
        _outputgates_add.resize(maxsize);
        _outputgates.resize(maxsize);

        _inputfilters.resize(maxsize);
        _forgetfilters.resize(maxsize);
        _cells.resize(maxsize);
        _halfhiddens.resize(maxsize);
        _hiddens.resize(maxsize);
    }

    //whether vectors have been allocated
    bool empty() {
        return _hiddens.empty();
    }

    void clear() {
        _inputgates_hidden.clear();
        _inputgates_input.clear();
        _inputgates_add.clear();
        _inputgates.clear();
        _forgetgates_hidden.clear();
        _forgetgates_input.clear();
        _forgetgates_add.clear();
        _forgetgates.clear();
        _halfcells_hidden.clear();
        _halfcells_input.clear();
        _halfcells_add.clear();
        _halfcells.clear();
        _outputgates_hidden.clear();
        _outputgates_input.clear();
        _outputgates_add.clear();
        _outputgates.clear();

        _inputfilters.clear();
        _forgetfilters.clear();
        _cells.clear();
        _halfhiddens.clear();
        _hiddens.clear();

        _left2right = true;
        _param = NULL;
        _nSize = 0;
        _inDim = 0;
        _outDim = 0;
    }

public:
    void forward(Graph *cg, const vector<PNode>& x) {
        if (x.size() == 0) {
            std::cout << "empty inputs for lstm operation" << std::endl;
            return;
        }
        _nSize = x.size();
        if (x[0]->val.dim != _inDim) {
            std::cout << "input dim does not match for lstm operation" << std::endl;
            return;
        }

        int idx_begin = _left2right ? 0 : x.size() - 1;
        int idx_step = _left2right ? 1 : -1;

        for (int idx = idx_begin; _left2right ? idx < _nSize : idx >= 0; idx += idx_step) {
            if (idx == idx_begin) {
                _bucket.forward(cg, 0);
                _inputgates_hidden[idx].forward(cg, &_bucket);
                _inputgates_input[idx].forward(cg, x[idx]);
                _inputgates_add[idx].forward(cg, &_inputgates_hidden[idx], &_inputgates_input[idx]);
                _inputgates[idx].forward(cg, &_inputgates_add[idx]);

                _outputgates_hidden[idx].forward(cg, &_bucket);
                _outputgates_input[idx].forward(cg, x[idx]);
                _outputgates_add[idx].forward(cg, &_outputgates_hidden[idx], &_outputgates_input[idx]);
                _outputgates[idx].forward(cg, &_outputgates_add[idx]);

                _halfcells_hidden[idx].forward(cg, &_bucket);
                _halfcells_input[idx].forward(cg, x[idx]);
                _halfcells_add[idx].forward(cg, &_halfcells_hidden[idx], &_halfcells_input[idx]);
                _halfcells[idx].forward(cg, &_halfcells_add[idx]);

                _inputfilters[idx].forward(cg, &_halfcells[idx], &_inputgates[idx]);
                _cells[idx].forward(cg, &_inputfilters[idx], &_bucket);
                _halfhiddens[idx].forward(cg, &_cells[idx]);
                _hiddens[idx].forward(cg, &_halfhiddens[idx], &_outputgates[idx]);
            } else {
                _inputgates_hidden[idx].forward(cg, &_hiddens[idx - idx_step]);
                _inputgates_input[idx].forward(cg, x[idx]);
                _inputgates_add[idx].forward(cg, &_inputgates_hidden[idx], &_inputgates_input[idx]);
                _inputgates[idx].forward(cg, &_inputgates_add[idx]);

                _outputgates_hidden[idx].forward(cg, &_hiddens[idx - idx_step]);
                _outputgates_input[idx].forward(cg, x[idx]);
                _outputgates_add[idx].forward(cg, &_outputgates_hidden[idx], &_outputgates_input[idx]);
                _outputgates[idx].forward(cg, &_outputgates_add[idx]);

                _halfcells_hidden[idx].forward(cg, &_hiddens[idx - idx_step]);
                _halfcells_input[idx].forward(cg, x[idx]);
                _halfcells_add[idx].forward(cg, &_halfcells_hidden[idx], &_halfcells_input[idx]);
                _halfcells[idx].forward(cg, &_halfcells_add[idx]);

                _forgetgates_hidden[idx].forward(cg, &_hiddens[idx - idx_step]);
                _forgetgates_input[idx].forward(cg, x[idx]);
                _forgetgates_add[idx].forward(cg, &_forgetgates_hidden[idx], &_forgetgates_input[idx]);
                _forgetgates[idx].forward(cg, &_forgetgates_add[idx]);

                _inputfilters[idx].forward(cg, &_halfcells[idx], &_inputgates[idx]);
                _forgetfilters[idx].forward(cg, &_cells[idx - idx_step], &_forgetgates[idx]);
                _cells[idx].forward(cg, &_inputfilters[idx], &_forgetfilters[idx]);
                _halfhiddens[idx].forward(cg, &_cells[idx]);
                _hiddens[idx].forward(cg, &_halfhiddens[idx], &_outputgates[idx]);
            }
        }
    }
};
#endif
