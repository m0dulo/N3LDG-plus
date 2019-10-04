#ifndef ATTENTION_BUILDER
#define ATTENTION_BUILDER

/*
*  Attention.h:
*  a set of attention builders
*
*  Created on: Apr 22, 2017
*      Author: mszhang
*/

#include "MyLib.h"
#include "Node.h"
#include "BiOP.h"
#include "UniOP.h"
#include "Graph.h"
#include "AttentionHelp.h"
#include <memory>
#include <boost/format.hpp>

struct DotAttentionParams : public N3LDGSerializable
#if USE_GPU
, public TransferableComponents
#endif
{
    UniParams uni1;
    UniParams uni2;
    int hidden_dim;
    int guide_dim;

    DotAttentionParams() = default;

    void exportAdaParams(ModelUpdate& ada) {
        uni1.exportAdaParams(ada);
        uni2.exportAdaParams(ada);
    }

    void init(int nHidden, int nGuide) {
        uni1.init(1, nHidden, false);
        uni2.init(1, nGuide, false);
        hidden_dim = nHidden;
        guide_dim = nGuide;
    }

    Json::Value toJson() const override {
        Json::Value json;
        json["uni1"] = uni1.toJson();
        json["uni2"] = uni2.toJson();
        json["hidden_dim"] = hidden_dim;
        json["guide_dim"] = guide_dim;
        return json;
    }

    void fromJson(const Json::Value &json) override {
        uni1.fromJson(json["uni1"]);
        uni2.fromJson(json["uni2"]);
        hidden_dim = json["hidden_dim"].asInt();
        guide_dim = json["guide_dim"].asInt();
    }

#if USE_GPU
    std::vector<Transferable *> transferablePtrs() override {
        return {&uni1, &uni2};
    }

    virtual std::string name() const {
        return "AttentionParams";
    }
#endif
};

class DotAttentionBuilder {
public:
    vector<Node *> _weights;
    Node* _hidden;

    DotAttentionParams* _param = nullptr;

    void init(DotAttentionParams &paramInit) {
        _param = &paramInit;
    }

    void forward(Graph &cg, vector<Node *>& x, Node& guide) {
        using namespace n3ldg_plus;
        if (x.size() == 0) {
            std::cerr << "empty inputs for lstm operation" << std::endl;
            abort();
        }

        if (x.at(0)->getDim() != _param->hidden_dim || guide.getDim() != _param->guide_dim) {
            std::cerr << "input dim does not match for attention  operation" << std::endl;
            cerr << boost::format("x.at(0)->dim:%1%, _param->hidden_dim:%2% guide.dim:%3% _param->guide_dim:%4%") % x.at(0)->getDim() % _param->hidden_dim % guide.getDim() % _param->guide_dim << endl;
            abort();
        }

        for (int idx = 0; idx < x.size(); idx++) {
//            BiNode* intermediate_node(new BiNode);
//            intermediate_node->init(1);
//            intermediate_node->setParam(_param->bi_atten);
//            intermediate_node->forward(cg, *x.at(idx), guide);
            Node *uni1 = n3ldg_plus::uni(cg, 1, _param->uni1, *x.at(idx));
            Node *uni2 = n3ldg_plus::uni(cg, 1, _param->uni2, guide);

            _weights.push_back(n3ldg_plus::add(cg, {uni1, uni2}));
        }
        _hidden = attention(cg, x, _weights);
    }
};

struct AttentionParams : public N3LDGSerializable
#if USE_GPU
, public TransferableComponents
#endif
{
    BiParams bi_atten;
    UniParams to_scalar_params;
    int hidden_dim;
    int guide_dim;

    AttentionParams() = default;

    void exportAdaParams(ModelUpdate& ada) {
        bi_atten.exportAdaParams(ada);
    }

    void init(int nHidden, int nGuide) {
        bi_atten.init(nHidden + nGuide, nHidden, nGuide, true);
        to_scalar_params.init(1, nHidden + nGuide, false);
        hidden_dim = nHidden;
        guide_dim = nGuide;
    }

    Json::Value toJson() const override {
        Json::Value json;
        json["bi_atten"] = bi_atten.toJson();
        json["to_scalar_params"] = to_scalar_params.toJson();
        json["hidden_dim"] = hidden_dim;
        json["guide_dim"] = guide_dim;
        return json;
    }

    void fromJson(const Json::Value &json) override {
        bi_atten.fromJson(json["bi_atten"]);
        to_scalar_params.fromJson(json["to_scalar_params"]);
        hidden_dim = json["hidden_dim"].asInt();
        guide_dim = json["guide_dim"].asInt();
    }

#if USE_GPU
    std::vector<Transferable *> transferablePtrs() override {
        return {&bi_atten, &to_scalar_params};
    }

    virtual std::string name() const {
        return "AttentionParams";
    }
#endif
};

class AttentionBuilder {
public:
    vector<LinearNode *> _weights;
    vector<BiNode *> _intermediate_nodes;
    Node* _hidden = nullptr;

    AttentionParams* _param = nullptr;

    void init(AttentionParams &paramInit) {
        _param = &paramInit;
        _hidden->init(paramInit.hidden_dim);
    }

    void forward(Graph &cg, vector<Node *>& x, Node& guide) {
        using namespace n3ldg_plus;
        if (x.size() == 0) {
            std::cerr << "empty inputs for lstm operation" << std::endl;
            abort();
        }

        if (x.at(0)->getDim() != _param->hidden_dim || guide.getDim() != _param->guide_dim) {
            std::cerr << "input dim does not match for attention  operation" << std::endl;
            cerr << boost::format("x.at(0)->dim:%1%, _param->hidden_dim:%2% guide.dim:%3% _param->guide_dim:%4%") % x.at(0)->getDim() % _param->hidden_dim % guide.getDim() % _param->guide_dim << endl;
            abort();
        }

        for (int idx = 0; idx < x.size(); idx++) {
            BiNode* intermediate_node(new BiNode);
            intermediate_node->setParam(_param->bi_atten);
            intermediate_node->init(_param->guide_dim + _param->hidden_dim);
            intermediate_node->forward(cg, *x.at(idx), guide);
            _intermediate_nodes.push_back(intermediate_node);

            LinearNode* uni_node(new LinearNode);
            uni_node->setParam(_param->to_scalar_params);
            uni_node->init(1);
            uni_node->forward(cg, *intermediate_node);
            _weights.push_back(uni_node);
        }
        vector<Node *> weights = toNodePointers<LinearNode>(_weights);
        _hidden = attention(cg, x, weights);
    }
};

#endif
