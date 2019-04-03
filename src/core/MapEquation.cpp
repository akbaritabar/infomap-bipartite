/*
 * MapEquation.h
 *
 *  Created on: 25 feb 2015
 *      Author: Daniel
 */
#include "MapEquation.h"
#include "../utils/infomath.h"
#include "../io/convert.h"
#include "../io/Config.h"
#include "../utils/Log.h"
#include "../utils/VectorMap.h"
#include "NodeBase.h"
#include "Node.h"
#include "FlowData.h"
#include <vector>
#include <map>
#include <iostream>

namespace infomap {

	using NodeType = Node<FlowData>;

// ===================================================
// Getters
// ===================================================


// ===================================================
// IO
// ===================================================

std::ostream& MapEquation::print(std::ostream& out) const {
	return out << "(" << indexCodelength.first << ", " << indexCodelength.second << ") + (" << moduleCodelength.first << ", " << moduleCodelength.second << ") = " << io::toPrecision(infomath::total(codelength));
}

// std::ostream& operator<<(std::ostream& out, const MapEquation& mapEq) {
// 	return mapEq.print(out);
// }


// ===================================================
// Init
// ===================================================

void MapEquation::init(const Config& config)
{
	Log(3) << "MapEquation::init()...\n";
}

void MapEquation::initNetwork(NodeBase& root)
{
	Log(3) << "MapEquation::initNetwork()...\n";

	nodeFlow_log_nodeFlow = { 0.0, 0.0 };
	for (NodeBase& node : root)
	{
		nodeFlow_log_nodeFlow += infomath::plogp(getNode(node).data.flow);
	}
	initSubNetwork(root);
}

void MapEquation::initSuperNetwork(NodeBase& root)
{
	Log(3) << "MapEquation::initSuperNetwork()...\n";

	nodeFlow_log_nodeFlow = { 0.0, 0.0 };
	for (NodeBase& node : root)
	{
		nodeFlow_log_nodeFlow += infomath::plogp(getNode(node).data.enterFlow);
	}
}

void MapEquation::initSubNetwork(NodeBase& root)
{
	exitNetworkFlow = getNode(root).data.exitFlow;
	exitNetworkFlow_log_exitNetworkFlow = infomath::plogp(exitNetworkFlow);
}

void MapEquation::initPartition(std::vector<NodeBase*>& nodes)
{
	calculateCodelength(nodes);
}


// ===================================================
// Codelength
// ===================================================

void MapEquation::calculateCodelength(std::vector<NodeBase*>& nodes)
{
	calculateCodelengthTerms(nodes);

	calculateCodelengthFromCodelengthTerms();
}

void MapEquation::calculateCodelengthTerms(std::vector<NodeBase*>& nodes)
{
	enter_log_enter = { 0.0, 0.0 };
	flow_log_flow = { 0.0, 0.0 };
	exit_log_exit = { 0.0, 0.0 };
	enterFlow = { 0.0, 0.0 };

	// For each module
	for (NodeBase* n : nodes)
	{
		const NodeType& node = getNode(*n);
		// own node/module codebook
		flow_log_flow += infomath::plogp(node.data.flow + node.data.exitFlow);

		// use of index codebook
		enter_log_enter += infomath::plogp(node.data.enterFlow);
		exit_log_exit += infomath::plogp(node.data.exitFlow);
		enterFlow += node.data.enterFlow;

		Log() << "[DEBUG] node " << node.index << ": (" << node.data.flow.first << ", " << node.data.flow.second << ")\n";
		Log() << "[DEBUG] node " << node.index << ": (" << node.data.exitFlow.first << ", " << node.data.exitFlow.second << ")\n";
	}
	enterFlow += exitNetworkFlow;
	enterFlow_log_enterFlow = infomath::plogp(enterFlow);

	Log() << "[DEBUG] enter_log_enter: (" << enter_log_enter.first << ", " << enter_log_enter.second << ")\n";
	Log() << "[DEBUG] flow_log_flow:   (" << flow_log_flow.first << ", " << flow_log_flow.second << ")\n";
	Log() << "[DEBUG] exit_log_exit:   (" << exit_log_exit.first << ", " << exit_log_exit.second << ")\n";
	Log() << "[DEBUG] enterFlow:       (" << enterFlow.first << ", " << enterFlow.second << ")\n";
}

void MapEquation::calculateCodelengthFromCodelengthTerms()
{
	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;

	Log() << "[DEBUG] Codelength: (" << indexCodelength.first << ", " << indexCodelength.second << ") + (" << moduleCodelength.first << ", " << moduleCodelength.second << ") = (" << codelength.first << ", " << codelength.second << ")\n";
}

std::pair<double, double> MapEquation::calcCodelength(const NodeBase& parent) const
{
	return parent.isLeafModule()? calcCodelengthOnModuleOfLeafNodes(parent) : calcCodelengthOnModuleOfModules(parent);
}

std::pair<double, double> MapEquation::calcCodelengthOnModuleOfLeafNodes(const NodeBase& p) const
{
	auto& parent = getNode(p);
	std::pair<double, double> parentFlow = parent.data.flow;
	std::pair<double, double> parentExit = parent.data.exitFlow;
	std::pair<double, double> totalParentFlow = parentFlow + parentExit;
	if (infomath::total(totalParentFlow) < 1e-16)
		return { 0.0, 0.0 };

	//Log() << "[DEBUG] parentFlow: (" << parentFlow.first << ", " << parentFlow.second << ")\n";
	//Log() << "[DEBUG] parentExit: (" << parentExit.first << ", " << parentExit.second << ")\n";
	//Log() << "[DEBUG] totalParentFlow: (" << totalParentFlow.first << ", " << totalParentFlow.second << ")\n";

	std::pair<double, double> indexLength = { 0.0, 0.0 };
	for (const auto& node : parent)
	{
		//std::pair<double, double> p = infomath::plogp(getNode(node).data.flow / totalParentFlow);
		//Log() << "[DEBUG] node " << node.index << ": (" << p.first << ", " << p.second << ")\n";
		indexLength -= infomath::plogp(getNode(node).data.flow / totalParentFlow);
	}
	indexLength -= infomath::plogp(parentExit / totalParentFlow);

	indexLength *= totalParentFlow;

	//Log() << "[DEBUG] indexLength: (" << indexLength.first << ", " << indexLength.second << ")\n";

	return indexLength;
}

std::pair<double, double> MapEquation::calcCodelengthOnModuleOfModules(const NodeBase& p) const
{
	auto& parent = getNode(p);
	std::pair<double, double> parentFlow = parent.data.flow;
	std::pair<double, double> parentExit = parent.data.exitFlow;
	if (infomath::total(parentFlow) < 1e-16)
		return { 0.0, 0.0 };

	// H(x) = -xlog(x), T = q + SUM(p), q = exitFlow, p = enterFlow
	// Normal format
	// L = q * -log(q/T) + p * SUM(-log(p/T))
	// Compact format
	// L = T * ( H(q/T) + SUM( H(p/T) ) )
	// Expanded format
	// L = q * -log(q) - q * -log(T) + SUM( p * -log(p) - p * -log(T) )
	//   = T * log(T) - q*log(q) - SUM( p*log(p) )
	// As T is not known, use expanded format to avoid two loops
	std::pair<double, double> sumEnter = { 0.0, 0.0 };
	std::pair<double, double> sumEnterLogEnter = { 0.0, 0.0 };
	for (const auto& n : parent)
	{
		auto& node = getNode(n);
		sumEnter += node.data.enterFlow; // rate of enter to finer level
		sumEnterLogEnter += infomath::plogp(node.data.enterFlow);
	}
	// The possibilities from this module: Either exit to coarser level or enter one of its children
	std::pair<double, double> totalCodewordUse = parentExit + sumEnter;

	return infomath::plogp(totalCodewordUse) - sumEnterLogEnter - infomath::plogp(parentExit);
}


double MapEquation::getDeltaCodelengthOnMovingNode(NodeBase& curr,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	using infomath::plogp;
	auto& current = getNode(curr);
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	std::pair<double, double> deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	std::pair<double, double> deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	std::pair<double, double> delta_enter = plogp(enterFlow + deltaEnterExitOldModule - deltaEnterExitNewModule) - enterFlow_log_enterFlow;

	std::pair<double, double> delta_enter_log_enter = \
			- plogp(moduleFlowData[oldModule].enterFlow) \
			- plogp(moduleFlowData[newModule].enterFlow) \
			+ plogp(moduleFlowData[oldModule].enterFlow - current.data.enterFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].enterFlow + current.data.enterFlow - deltaEnterExitNewModule);

	std::pair<double, double> delta_exit_log_exit = \
			- plogp(moduleFlowData[oldModule].exitFlow) \
			- plogp(moduleFlowData[newModule].exitFlow) \
			+ plogp(moduleFlowData[oldModule].exitFlow - current.data.exitFlow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + current.data.exitFlow - deltaEnterExitNewModule);

	std::pair<double, double> delta_flow_log_flow = \
			- plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) \
			- plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow) \
			+ plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow \
					- current.data.exitFlow - current.data.flow + deltaEnterExitOldModule) \
			+ plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow \
					+ current.data.exitFlow + current.data.flow - deltaEnterExitNewModule);

	std::pair<double, double> deltaL = delta_enter - delta_enter_log_enter - delta_exit_log_exit + delta_flow_log_flow;
	return infomath::total(deltaL);
}

void MapEquation::updateCodelengthOnMovingNode(NodeBase& curr,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	using infomath::plogp;
	auto& current = getNode(curr);
	unsigned int oldModule = oldModuleDelta.module;
	unsigned int newModule = newModuleDelta.module;
	std::pair<double, double> deltaEnterExitOldModule = oldModuleDelta.deltaEnter + oldModuleDelta.deltaExit;
	std::pair<double, double> deltaEnterExitNewModule = newModuleDelta.deltaEnter + newModuleDelta.deltaExit;

	enterFlow -= \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	enter_log_enter -= \
			plogp(moduleFlowData[oldModule].enterFlow) + \
			plogp(moduleFlowData[newModule].enterFlow);
	exit_log_exit -= \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	flow_log_flow -= \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);


	moduleFlowData[oldModule] -= current.data;
	moduleFlowData[newModule] += current.data;

	moduleFlowData[oldModule].enterFlow += deltaEnterExitOldModule;
	moduleFlowData[oldModule].exitFlow += deltaEnterExitOldModule;
	moduleFlowData[newModule].enterFlow -= deltaEnterExitNewModule;
	moduleFlowData[newModule].exitFlow -= deltaEnterExitNewModule;

	enterFlow += \
			moduleFlowData[oldModule].enterFlow + \
			moduleFlowData[newModule].enterFlow;
	enter_log_enter += \
			plogp(moduleFlowData[oldModule].enterFlow) + \
			plogp(moduleFlowData[newModule].enterFlow);
	exit_log_exit += \
			plogp(moduleFlowData[oldModule].exitFlow) + \
			plogp(moduleFlowData[newModule].exitFlow);
	flow_log_flow += \
			plogp(moduleFlowData[oldModule].exitFlow + moduleFlowData[oldModule].flow) + \
			plogp(moduleFlowData[newModule].exitFlow + moduleFlowData[newModule].flow);

	enterFlow_log_enterFlow = plogp(enterFlow);

	indexCodelength = enterFlow_log_enterFlow - enter_log_enter - exitNetworkFlow_log_exitNetworkFlow;
	moduleCodelength = -exit_log_exit + flow_log_flow - nodeFlow_log_nodeFlow;
	codelength = indexCodelength + moduleCodelength;
}

NodeBase* MapEquation::createNode() const
{
	return new NodeType();
}

NodeBase* MapEquation::createNode(const NodeBase& other) const
{
	return new NodeType(static_cast<const NodeType&>(other));
}

NodeBase* MapEquation::createNode(FlowDataType flowData) const
{
	return new NodeType(static_cast<const NodeType&>(flowData));
}

const NodeType& MapEquation::getNode(const NodeBase& other) const
{
	return static_cast<const NodeType&>(other);
}

// ===================================================
// Debug
// ===================================================

void MapEquation::printDebug()
{
	std::cout << "(enterFlow_log_enterFlow: (" << enterFlow_log_enterFlow.first << ", " << enterFlow_log_enterFlow.second << "), " <<
			"enter_log_enter: (" << enter_log_enter.first << ", " << enter_log_enter.second << "), " <<
			"exitNetworkFlow_log_exitNetworkFlow: (" << exitNetworkFlow_log_exitNetworkFlow.first << ", " << exitNetworkFlow_log_exitNetworkFlow.second << ")) ";
//	std::cout << "enterFlow_log_enterFlow: " << enterFlow_log_enterFlow << "\n" <<
//			"enter_log_enter: " << enter_log_enter << "\n" <<
//			"exitNetworkFlow_log_exitNetworkFlow: " << exitNetworkFlow_log_exitNetworkFlow << "\n";
//	std::cout << "exit_log_exit: " << exit_log_exit << "\n" <<
//			"flow_log_flow: " << flow_log_flow << "\n" <<
//			"nodeFlow_log_nodeFlow: " << nodeFlow_log_nodeFlow << "\n";
}

}
