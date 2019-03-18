/*
 * MemMapEquation.h
 *
 *  Created on: 4 mar 2015
 *      Author: Daniel
 */

#include "MemMapEquation.h"
#include "FlowData.h"
#include "NodeBase.h"
#include "Node.h"
#include "../utils/Log.h"
#include <vector>
#include <set>
#include <map>
#include <utility>

namespace infomap {

	using NodeType = Node<FlowData>;

// ===================================================
// IO
// ===================================================

std::ostream& MemMapEquation::print(std::ostream& out) const {
	return out << indexCodelength << " + " << moduleCodelength << " = " <<	io::toPrecision(codelength);
}

// std::ostream& operator<<(std::ostream& out, const MemMapEquation& mapEq) {
// 	return out << indexCodelength << " + " << moduleCodelength << " = " <<	io::toPrecision(codelength);
// }


// ===================================================
// Init
// ===================================================

void MemMapEquation::init(const Config& config)
{
	Log(3) << "MemMapEquation::init()...\n";
}


void MemMapEquation::initNetwork(NodeBase& root)
{
	initPhysicalNodes(root);
}

void MemMapEquation::initSuperNetwork(NodeBase& root)
{
	//TODO: How use enterFlow instead of flow
}

void MemMapEquation::initSubNetwork(NodeBase& root)
{
//	Base::initSubNetwork(root);
}

void MemMapEquation::initPartition(std::vector<NodeBase*>& nodes)
{
	initPartitionOfPhysicalNodes(nodes);

	calculateCodelength(nodes);
}

void MemMapEquation::initPhysicalNodes(NodeBase& root)
{
	bool notInitiated = root.firstChild->physicalNodes.empty();
	if (notInitiated)
	{
		Log(3) << "MemMapEquation::initPhysicalNodesOnOriginalNetwork()...\n";
		std::set<unsigned int> setOfPhysicalNodes;
		// Collect all physical nodes in this network
		for (NodeBase& node : root)
		{
			setOfPhysicalNodes.insert(node.physicalId);
		}

		m_numPhysicalNodes = setOfPhysicalNodes.size();

		// Re-index physical nodes
		std::map<unsigned int, unsigned int> toZeroBasedIndex;
		unsigned int zeroBasedPhysicalId = 0;
		for (unsigned int physIndex : setOfPhysicalNodes)
		{
			toZeroBasedIndex.insert(std::make_pair(physIndex, zeroBasedPhysicalId++));
		}

		for (NodeBase& node : root)
		{
			unsigned int zeroBasedIndex = toZeroBasedIndex[node.physicalId];
			node.physicalNodes.push_back(PhysData(zeroBasedIndex, node.getFlow()));
		}
	}
	else
	{
		Log(3) << "MemMapEquation::initPhysicalNodesOnSubNetwork()...\n";
		std::set<unsigned int> setOfPhysicalNodes;

		// Collect all physical nodes in this sub network
		for (NodeBase& node : root)
		{
			for (PhysData& physData : node.physicalNodes)
			{
				setOfPhysicalNodes.insert(physData.physNodeIndex);
			}
		}

		m_numPhysicalNodes = setOfPhysicalNodes.size();

		// Re-index physical nodes
		std::map<unsigned int, unsigned int> toZeroBasedIndex;
		unsigned int zeroBasedPhysicalId = 0;
		for (unsigned int physIndex : setOfPhysicalNodes)
		{
			toZeroBasedIndex.insert(std::make_pair(physIndex, zeroBasedPhysicalId++));
		}

		for (NodeBase& node : root)
		{
			for (PhysData& physData : node.physicalNodes)
			{
				physData.physNodeIndex = toZeroBasedIndex[physData.physNodeIndex];
			}
		}
	}
}

void MemMapEquation::initPartitionOfPhysicalNodes(std::vector<NodeBase*>& nodes)
{
	Log(4) << "MemMapEquation::initPartitionOfPhysicalNodes()...\n";
	m_physToModuleToMemNodes.clear();
	m_physToModuleToMemNodes.resize(m_numPhysicalNodes);

	for (auto& n : nodes)
	{
		NodeBase& node = *n;
		unsigned int moduleIndex = node.index; // Assume unique module index for all nodes in this initiation phase

		for(PhysData& physData : node.physicalNodes)
		{
			m_physToModuleToMemNodes[physData.physNodeIndex].insert(m_physToModuleToMemNodes[physData.physNodeIndex].end(),
					std::make_pair(moduleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
		}
	}

	m_memoryContributionsAdded = false;
}



// ===================================================
// Codelength
// ===================================================

void MemMapEquation::calculateCodelength(std::vector<NodeBase*>& nodes)
{
	calculateCodelengthTerms(nodes);

	calculateNodeFlow_log_nodeFlow();

	calculateCodelengthFromCodelengthTerms();
}

void MemMapEquation::calculateNodeFlow_log_nodeFlow()
{
	nodeFlow_log_nodeFlow = 0.0;
	for (unsigned int i = 0; i < m_numPhysicalNodes; ++i)
	{
		const ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[i];
		for (ModuleToMemNodes::const_iterator modToMemIt(moduleToMemNodes.begin()); modToMemIt != moduleToMemNodes.end(); ++modToMemIt)
			nodeFlow_log_nodeFlow += infomath::plogp(modToMemIt->second.sumFlow);
	}
}

double MemMapEquation::calcCodelength(const NodeBase& parent) const
{
	return parent.isLeafModule() ?
		( parent.isRoot() ?
			// Use first-order model for one-level codebook
			MapEquation::calcCodelengthOnModuleOfLeafNodes(parent) :
			calcCodelengthOnModuleOfLeafNodes(parent)
		) :
		// Use first-order model on index codebook
		MapEquation::calcCodelengthOnModuleOfModules(parent);
}

double MemMapEquation::calcCodelengthOnModuleOfLeafNodes(const NodeBase& p) const
{
	auto& parent = getNode(p);
	if (parent.numPhysicalNodes() == 0) {
		std::cout << "(*)";
		return MapEquation::calcCodelength(parent); // Infomap root node
	}

	//TODO: For ordinary networks, flow should be used instead of enter flow
	// for leaf nodes, what about memory networks? sumFlowFromM2Node vs sumEnterFlowFromM2Node?
	double parentFlow = parent.data.flow;
	double parentExit = parent.data.exitFlow;
	double totalParentFlow = parentFlow + parentExit;
	if (totalParentFlow < 1e-16)
		return 0.0;

	double indexLength = 0.0;

	for (const PhysData& physData : parent.physicalNodes)
	{
		indexLength -= infomath::plogp(physData.sumFlowFromM2Node / totalParentFlow);
	}
	indexLength -= infomath::plogp(parentExit / totalParentFlow);

	indexLength *= totalParentFlow;

	return indexLength;
}

void MemMapEquation::addMemoryContributions(NodeBase& current,
	DeltaFlowDataType& oldModuleDelta, VectorMap<DeltaFlowDataType>& moduleDeltaFlow)
{
	// Overlapping modules
	/**
	 * delta = old.first + new.first + old.second - new.second.
	 * Two cases: (p(x) = plogp(x))
	 * Moving to a module that already have that physical node: (old: p1, p2, new p3, moving p2 -> old:p1, new p2,p3)
	 * Then old.second = new.second = plogp(physicalNodeSize) -> cancelation -> delta = p(p1) - p(p1+p2) + p(p2+p3) - p(p3)
	 * Moving to a module that not have that physical node: (old: p1, p2, new -, moving p2 -> old: p1, new: p2)
	 * Then new.first = new.second = 0 -> delta = p(p1) - p(p1+p2) + p(p2).
	 */
	auto& physicalNodes = current.physicalNodes;
	unsigned int numPhysicalNodes = physicalNodes.size();
	for (unsigned int i = 0; i < numPhysicalNodes; ++i)
	{
		PhysData& physData = physicalNodes[i];
		ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];
		for (ModuleToMemNodes::iterator overlapIt(moduleToMemNodes.begin()); overlapIt != moduleToMemNodes.end(); ++overlapIt)
		{
			unsigned int moduleIndex = overlapIt->first;
			MemNodeSet& memNodeSet = overlapIt->second;
			if (moduleIndex == current.index) // From where the multiple assigned node is moved
			{
				double oldPhysFlow = memNodeSet.sumFlow;
				double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromM2Node;
				oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
				oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
			}
			else // To where the multiple assigned node is moved
			{
				double oldPhysFlow = memNodeSet.sumFlow;
				double newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromM2Node;

				// DeltaFlowDataType& otherDeltaFlow = moduleDeltaFlow[moduleIndex];
				// otherDeltaFlow.module = moduleIndex; // Make sure module index is correct if created new module link
				// otherDeltaFlow.sumDeltaPlogpPhysFlow = infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
				// otherDeltaFlow.sumPlogpPhysFlow = infomath::plogp(physData.sumFlowFromM2Node);
				// ++otherDeltaFlow.count;
				double sumDeltaPlogpPhysFlow = infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
				double sumPlogpPhysFlow = infomath::plogp(physData.sumFlowFromM2Node);
				moduleDeltaFlow.add(moduleIndex, DeltaFlowDataType(moduleIndex, 0.0, 0.0, sumDeltaPlogpPhysFlow, sumPlogpPhysFlow));
			}
		}
	}
	m_memoryContributionsAdded = true;
}


double MemMapEquation::getDeltaCodelengthOnMovingNode(NodeBase& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	double deltaL = Base::getDeltaCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);

	double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;

	return deltaL - delta_nodeFlow_log_nodeFlow;
}




// ===================================================
// Consolidation
// ===================================================

void MemMapEquation::updateCodelengthOnMovingNode(NodeBase& current,
		DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta, std::vector<FlowDataType>& moduleFlowData, std::vector<unsigned int>& moduleMembers)
{
	Base::updateCodelengthOnMovingNode(current, oldModuleDelta, newModuleDelta, moduleFlowData, moduleMembers);
	if (m_memoryContributionsAdded)
		updatePhysicalNodes(current, oldModuleDelta.module, newModuleDelta.module);
	else
		addMemoryContributionsAndUpdatePhysicalNodes(current, oldModuleDelta, newModuleDelta);

	double delta_nodeFlow_log_nodeFlow = oldModuleDelta.sumDeltaPlogpPhysFlow + newModuleDelta.sumDeltaPlogpPhysFlow + oldModuleDelta.sumPlogpPhysFlow - newModuleDelta.sumPlogpPhysFlow;

	nodeFlow_log_nodeFlow += delta_nodeFlow_log_nodeFlow;
	moduleCodelength -= delta_nodeFlow_log_nodeFlow;
	codelength -= delta_nodeFlow_log_nodeFlow;
}


void MemMapEquation::updatePhysicalNodes(NodeBase& current, unsigned int oldModuleIndex, unsigned int bestModuleIndex)
{
	// For all multiple assigned nodes
	for (unsigned int i = 0; i < current.physicalNodes.size(); ++i)
	{
		PhysData& physData = current.physicalNodes[i];
		ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

		// Remove contribution to old module
		ModuleToMemNodes::iterator overlapIt = moduleToMemNodes.find(oldModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
			throw std::length_error(io::Str() << "Couldn't find old module " << oldModuleIndex << " in physical node " << physData.physNodeIndex);
		MemNodeSet& memNodeSet = overlapIt->second;
		memNodeSet.sumFlow -= physData.sumFlowFromM2Node;
		if (--memNodeSet.numMemNodes == 0)
			moduleToMemNodes.erase(overlapIt);

		// Add contribution to new module
		overlapIt = moduleToMemNodes.find(bestModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
			moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
		else {
			MemNodeSet& memNodeSet = overlapIt->second;
			++memNodeSet.numMemNodes;
			memNodeSet.sumFlow += physData.sumFlowFromM2Node;
		}
	}
}

void MemMapEquation::addMemoryContributionsAndUpdatePhysicalNodes(NodeBase& current, DeltaFlowDataType& oldModuleDelta, DeltaFlowDataType& newModuleDelta)
{
	unsigned int oldModuleIndex = oldModuleDelta.module;
	unsigned int bestModuleIndex = newModuleDelta.module;

	// For all multiple assigned nodes
	for (unsigned int i = 0; i < current.physicalNodes.size(); ++i)
	{
		PhysData& physData = current.physicalNodes[i];
		ModuleToMemNodes& moduleToMemNodes = m_physToModuleToMemNodes[physData.physNodeIndex];

		// Remove contribution to old module
		ModuleToMemNodes::iterator overlapIt = moduleToMemNodes.find(oldModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
			throw std::length_error("Couldn't find old module among physical node assignments.");
		MemNodeSet& memNodeSet = overlapIt->second;
		double oldPhysFlow = memNodeSet.sumFlow;
		double newPhysFlow = memNodeSet.sumFlow - physData.sumFlowFromM2Node;
		oldModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
		oldModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
		memNodeSet.sumFlow -= physData.sumFlowFromM2Node;
		if (--memNodeSet.numMemNodes == 0)
			moduleToMemNodes.erase(overlapIt);


		// Add contribution to new module
		overlapIt = moduleToMemNodes.find(bestModuleIndex);
		if (overlapIt == moduleToMemNodes.end())
		{
			moduleToMemNodes.insert(std::make_pair(bestModuleIndex, MemNodeSet(1, physData.sumFlowFromM2Node)));
			oldPhysFlow = 0.0;
			newPhysFlow = physData.sumFlowFromM2Node;
			newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
			newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
		}
		else
		{
			MemNodeSet& memNodeSet = overlapIt->second;
			oldPhysFlow = memNodeSet.sumFlow;
			newPhysFlow = memNodeSet.sumFlow + physData.sumFlowFromM2Node;
			newModuleDelta.sumDeltaPlogpPhysFlow += infomath::plogp(newPhysFlow) - infomath::plogp(oldPhysFlow);
			newModuleDelta.sumPlogpPhysFlow += infomath::plogp(physData.sumFlowFromM2Node);
			++memNodeSet.numMemNodes;
			memNodeSet.sumFlow += physData.sumFlowFromM2Node;
		}

	}
}


void MemMapEquation::consolidateModules(std::vector<NodeBase*>& modules)
{
	std::map<unsigned int, std::map<unsigned int, unsigned int> > validate;

	for(unsigned int i = 0; i < m_numPhysicalNodes; ++i)
	{
		ModuleToMemNodes& modToMemNodes = m_physToModuleToMemNodes[i];
		for(ModuleToMemNodes::iterator overlapIt = modToMemNodes.begin(); overlapIt != modToMemNodes.end(); ++overlapIt)
		{
			if(++validate[overlapIt->first][i] > 1)
				throw std::domain_error("[InfomapGreedy::consolidateModules] Error updating physical nodes: duplication error");

			modules[overlapIt->first]->physicalNodes.push_back(PhysData(i, overlapIt->second.sumFlow));
		}
	}
}


NodeBase* MemMapEquation::createNode() const
{
	return new NodeType();
}

NodeBase* MemMapEquation::createNode(const NodeBase& other) const
{
	return new NodeType(static_cast<const NodeType&>(other));
}

NodeBase* MemMapEquation::createNode(FlowDataType flowData) const
{
	return new NodeType(flowData);
}

const NodeType& MemMapEquation::getNode(const NodeBase& other) const
{
	return static_cast<const NodeType&>(other);
}

// ===================================================
// Debug
// ===================================================

void MemMapEquation::printDebug()
{
	std::cout << "MemMapEquation::m_numPhysicalNodes: " << m_numPhysicalNodes << "\n";
	Base::printDebug();
}


}
