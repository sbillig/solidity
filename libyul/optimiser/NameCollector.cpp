/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * Specific AST walker that collects all defined names.
 */

#include <libyul/optimiser/NameCollector.h>

#include <libyul/AST.h>
#include <libyul/Exceptions.h>
#include <libyul/Utilities.h>

using namespace solidity;
using namespace solidity::yul;
using namespace solidity::util;

ReferencesCounter::ReferencesCounter(ReferenceCounts& _referencesToSubtractFrom):
	m_referencesToSubtractFrom(&_referencesToSubtractFrom)
{}

void NameCollector::operator()(VariableDeclaration const& _varDecl)
{
	if (m_collectWhat != OnlyFunctions)
		for (auto const& var: _varDecl.variables)
			m_names.emplace(var.name);
}

void NameCollector::operator()(FunctionDefinition const& _funDef)
{
	if (m_collectWhat != OnlyVariables)
		m_names.emplace(_funDef.name);
	if (m_collectWhat != OnlyFunctions)
	{
		for (auto const& arg: _funDef.parameters)
			m_names.emplace(arg.name);
		for (auto const& ret: _funDef.returnVariables)
			m_names.emplace(ret.name);
	}
	ASTWalker::operator ()(_funDef);
}

void ReferencesCounter::operator()(Identifier const& _identifier)
{
	recordReference(_identifier.name);
}

void ReferencesCounter::operator()(FunctionCall const& _funCall)
{
	recordReference(functionNameToHandle(_funCall.functionName));
	ASTWalker::operator()(_funCall);
}

ReferencesCounter::ReferenceCounts ReferencesCounter::countReferences(Block const& _block)
{
	ReferencesCounter counter;
	counter(_block);
	return std::move(counter.m_references);
}

ReferencesCounter::ReferenceCounts ReferencesCounter::countReferences(FunctionDefinition const& _function)
{
	ReferencesCounter counter;
	counter(_function);
	return std::move(counter.m_references);
}

ReferencesCounter::ReferenceCounts ReferencesCounter::countReferences(Expression const& _expression)
{
	ReferencesCounter counter;
	counter.visit(_expression);
	return std::move(counter.m_references);
}

bool ReferencesCounter::subtractReferences(Block const& _block, ReferenceCounts& _references)
{
	ReferencesCounter counter{_references};
	counter(_block);
	return counter.m_subtractedReference;
}

bool ReferencesCounter::subtractReferences(Expression const& _expression, ReferenceCounts& _references)
{
	ReferencesCounter counter{_references};
	counter.visit(_expression);
	return counter.m_subtractedReference;
}

void ReferencesCounter::recordReference(FunctionHandle _reference)
{
	if (!m_referencesToSubtractFrom)
	{
		++m_references[_reference];
		return;
	}

	auto reference = m_referencesToSubtractFrom->find(_reference);
	assertThrow(reference != m_referencesToSubtractFrom->end(), OptimizerException, "");
	assertThrow(reference->second > 0, OptimizerException, "");
	--reference->second;
	m_subtractedReference = true;
}

void VariableReferencesCounter::operator()(Identifier const& _identifier)
{
	++m_references[_identifier.name];
}

std::map<YulName, size_t> VariableReferencesCounter::countReferences(Block const& _block)
{
	VariableReferencesCounter counter;
	counter(_block);
	return std::move(counter.m_references);
}

std::map<YulName, size_t> VariableReferencesCounter::countReferences(FunctionDefinition const& _function)
{
	VariableReferencesCounter counter;
	counter(_function);
	return std::move(counter.m_references);
}

std::map<YulName, size_t> VariableReferencesCounter::countReferences(Expression const& _expression)
{
	VariableReferencesCounter counter;
	counter.visit(_expression);
	return std::move(counter.m_references);
}

std::map<YulName, size_t> VariableReferencesCounter::countReferences(Statement const& _statement)
{
	VariableReferencesCounter counter;
	counter.visit(_statement);
	return std::move(counter.m_references);
}

void AssignmentsSinceContinue::operator()(ForLoop const& _forLoop)
{
	m_forLoopDepth++;
	ASTWalker::operator()(_forLoop);
	m_forLoopDepth--;
}

void AssignmentsSinceContinue::operator()(Continue const&)
{
	if (m_forLoopDepth == 0)
		m_continueFound = true;
}

void AssignmentsSinceContinue::operator()(Assignment const& _assignment)
{
	if (m_continueFound)
		for (auto const& var: _assignment.variableNames)
			m_names.emplace(var.name);
}

void AssignmentsSinceContinue::operator()(FunctionDefinition const&)
{
	yulAssert(false, "");
}

std::set<YulName> solidity::yul::assignedVariableNames(Block const& _code)
{
	std::set<YulName> names;
	forEach<Assignment const>(_code, [&](Assignment const& _assignment) {
		for (auto const& var: _assignment.variableNames)
			names.emplace(var.name);
	});
	return names;
}

std::map<YulName, FunctionDefinition const*> solidity::yul::allFunctionDefinitions(Block const& _block)
{
	std::map<YulName, FunctionDefinition const*> result;
	forEach<FunctionDefinition const>(_block, [&](FunctionDefinition const& _function) {
		result[_function.name] = &_function;
	});
	return result;
}
