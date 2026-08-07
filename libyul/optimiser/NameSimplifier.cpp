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

#include <libyul/optimiser/NameSimplifier.h>
#include <libyul/optimiser/NameCollector.h>
#include <libyul/AST.h>
#include <libyul/Dialect.h>
#include <libyul/YulName.h>
#include <libyul/optimiser/NameDispenser.h>
#include <libyul/optimiser/OptimizerUtilities.h>

#include <libsolutil/CommonData.h>

#include <array>
#include <regex>
#include <string_view>

using namespace solidity::yul;

NameSimplifier::NameSimplifier(OptimiserStepContext& _context, Block const& _ast):
	m_context(_context)
{
	for (YulName name: _context.reservedIdentifiers)
		m_translations[name] = name;

	for (YulName const& name: NameCollector(_ast).names())
		findSimplification(name);
}

void NameSimplifier::operator()(FunctionDefinition& _funDef)
{
	translate(_funDef.name);
	renameVariables(_funDef.parameters);
	renameVariables(_funDef.returnVariables);
	ASTModifier::operator()(_funDef);
}

void NameSimplifier::operator()(VariableDeclaration& _varDecl)
{
	renameVariables(_varDecl.variables);
	ASTModifier::operator()(_varDecl);
}

void NameSimplifier::renameVariables(std::vector<NameWithDebugData>& _variables)
{
	for (NameWithDebugData& typedName: _variables)
		translate(typedName.name);
}

void NameSimplifier::operator()(Identifier& _identifier)
{
	translate(_identifier.name);
}

void NameSimplifier::operator()(FunctionCall& _funCall)
{
	// The visitor on its own does not visit the function name.
	if (!isBuiltinFunctionCall(_funCall))
	{
		yulAssert(std::holds_alternative<Identifier>(_funCall.functionName));
		(*this)(std::get<Identifier>(_funCall.functionName));
	}
	ASTModifier::operator()(_funCall);
}

void NameSimplifier::findSimplification(YulName const& _name)
{
	if (m_translations.count(_name))
		return;

	std::string name = _name.str();

	static auto replacements = std::vector<std::pair<std::regex, std::string>>{
		{std::regex("_\\$|\\$_"), "_"}, // remove type mangling delimiters
		{std::regex("_[0-9]+([^0-9a-fA-Fx])"), "$1"}, // removes AST IDs that are not hex.
		{std::regex("_[0-9]+$"), ""}, // removes AST IDs that are not hex.
		{std::regex("_t_"), "_"}, // remove type prefixes
		{std::regex("__"), "_"},
		{std::regex("(abi_..code.*)_to_.*"), "$1"}, // removes _to... for abi functions
		{std::regex("(stringliteral_?[0-9a-f][0-9a-f][0-9a-f][0-9a-f])[0-9a-f]*"), "$1"}, // shorten string literal
		{std::regex("tuple_"), ""},
		{std::regex("_memory_ptr"), ""},
		{std::regex("_calldata_ptr"), "_calldata"},
		{std::regex("_fromStack"), ""},
		{std::regex("_storage_storage"), "_storage"},
		{std::regex("(storage.*)_?storage"), "$1"},
		{std::regex("_memory_memory"), "_memory"},
		{std::regex("_contract\\$_([^_]*)_?"), "$1_"},
		{std::regex("index_access_(t_)?array"), "index_access"},
		{std::regex("[0-9]*_$"), ""}
	};
	static constexpr std::array<std::string_view, 17> requiredSubstrings{
		"", "", "", "_t_", "__", "abi_", "stringliteral", "tuple_", "_memory_ptr",
		"_calldata_ptr", "_fromStack", "_storage_storage", "storage", "_memory_memory",
		"_contract$_", "index_access_", ""
	};

	for (size_t index = 0; index < replacements.size(); ++index)
	{
		auto containsUnderscoreDigit = [&]()
		{
			for (size_t character = 0; character + 1 < name.size(); ++character)
				if (
					name[character] == '_' &&
					name[character + 1] >= '0' &&
					name[character + 1] <= '9'
				)
					return true;
			return false;
		};
		bool couldMatch =
			index == 0 ? name.find("_$") != std::string::npos || name.find("$_") != std::string::npos :
			index == 1 || index == 2 ? containsUnderscoreDigit() :
			index == 16 ? !name.empty() && name.back() == '_' :
			name.find(requiredSubstrings[index]) != std::string::npos;
		if (!couldMatch)
			continue;

		auto const& [pattern, substitute] = replacements[index];
		std::string candidate = regex_replace(name, pattern, substitute);
		if (!candidate.empty() && !m_context.dispenser.illegalName(YulName(candidate)))
			name = candidate;
	}

	if (name != _name.str())
	{
		YulName newName{name};
		m_context.dispenser.markUsed(newName);
		m_translations[_name] = std::move(newName);
	}
}

void NameSimplifier::translate(YulName& _name)
{
	auto it = m_translations.find(_name);
	if (it != m_translations.end())
		_name = it->second;
}
