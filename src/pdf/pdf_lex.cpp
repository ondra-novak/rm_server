/*
 * pdf_lex.cpp
 *
 *  Created on: 9. 3. 2021
 *      Author: ondra
 */

#include "pdf_lex.h"

namespace pdf {

std::string_view delimiters = "()[]<>{}/%";

std::vector<KeywordDef> keywords = {
		{"R",SymbolType::reference_mark},
		{"endobj",SymbolType::endobj},
		{"endstream",SymbolType::endstream},
		{"f",SymbolType::free_obj},
		{"false",SymbolType::bool_false},
		{"n",SymbolType::used_obj},
		{"null",SymbolType::null},
		{"obj",SymbolType::obj},
		{"stream",SymbolType::stream},
		{"trailer",SymbolType::trailer},
		{"true",SymbolType::bool_true},
		{"xref",SymbolType::xref}
};

template class SymbolStream<int (*)()>;

}


