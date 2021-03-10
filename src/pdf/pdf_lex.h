/*
 * pdf_lex.h
 *
 *  Created on: 9. 3. 2021
 *      Author: ondra
 */

#ifndef SRC_PDF_PDF_LEX_H_
#define SRC_PDF_PDF_LEX_H_
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace pdf {


enum class SymbolType {
	not_set,		//not symbol, variable is unset
	eof,            //reached end of file (not %EOF, rather -1 in input)
	unknown, 		//unknown combination of character
	comment,  //%
	obj,
	endobj,
	dict_begin, //<<
	dict_end, //>>
	array_begin,
	array_end,
	name,  // /xxx
	string,
	hex_string,
	number,
	int_number,
	stream,
	endstream,
	reference_mark, //R
	xref,
	trailer,
	bool_true,
	bool_false,
	null,
	free_obj,
	used_obj,
};



class Symbol {
public:

	const SymbolType type:8;
	bool str_used = false;
	union {
		const std::string text;
		const double f;
		const std::int64_t i;
	};

	Symbol(Symbol &&other):type(other.type),str_used(other.str_used) {
		if (str_used) {
			new(const_cast<std::string *>(&text)) std::string(std::move(other.text));
			other.str_used = false;
		} else {
			if (type == SymbolType::number) const_cast<double &>(f) = other.f;
			else if (type == SymbolType::int_number) const_cast<std::int64_t &>(i) = other.i;
		}
	}

	~Symbol() {
		if (str_used) {
			text.std::string::~string();
		}
	}
	Symbol():type(SymbolType::not_set) {}
	Symbol(SymbolType type):type(type) {}
	Symbol(SymbolType type, const std::string_view &text):type(type),str_used(true),text(text) {}
	Symbol(SymbolType type, std::string &&text):type(type),str_used(true),text(std::move(text)) {}
	explicit Symbol(double number):type(SymbolType::number),f(number) {}
	explicit Symbol(std::int64_t number):type(SymbolType::int_number),i(number) {}



	bool isSymbol(SymbolType type) const {
		if (type == this->type) return true;
		if (type == SymbolType::number && (this->type == SymbolType::int_number)) return true;
		if (type == SymbolType::string && (this->type == SymbolType::hex_string)) return true;
		return false;
	}

	double getNumber() const {
		switch (type) {
		case SymbolType::number: return f;
		case SymbolType::int_number: return i;
		case SymbolType::hex_string:
		case SymbolType::string:  return std::strtod(text.c_str(),nullptr);
		default: return 0;
		}
	}


	std::int64_t getInt() const {
		switch (type) {
		case SymbolType::number: return static_cast<std::int64_t>(f);
		case SymbolType::int_number: return i;
		case SymbolType::hex_string:
		case SymbolType::string:  return std::strtoll(text.c_str(),nullptr,10);
		default: return 0;
		}
	}

};

template<typename Fn>
class SymbolStream {
public:
	SymbolStream(Fn &&fn):fn(std::forward<Fn>(fn)) {}


	int readChar();
	Symbol read();
	void putBack(int chr) {next = chr;}
	int readSWS();

	Fn &getStream() {return fn;}
	const Fn &getStream() const {return fn;}

protected:
	Fn fn;
	int next = -2;
	std::string buff;

	Symbol readName();
	Symbol readHex();
	Symbol readString();
	Symbol readNumber();
	Symbol readKeyword();
	Symbol readComment();
	void readToBuffer();

};

template<typename Fn>
SymbolStream<Fn> createSymbolStream(Fn &&fn) {
	return SymbolStream<Fn>(std::forward<Fn>(fn));
}

extern std::string_view delimiters;

template<typename Fn>
inline Symbol SymbolStream<Fn>::read() {
	buff.clear();
	int i = readSWS();
	int j;
	switch (i) {
	case -1: return SymbolType::eof;
	case '/': return readName();
	case '<':j = readChar();
			 if (j == '<') return SymbolType::dict_begin;
			 else {putBack(j);return readHex();}
			 break;
	case '>':
		j = readChar();
		if (j == '>') return SymbolType::dict_end;
		else {putBack(j);return SymbolType::unknown;}
		break;
	case '[': return SymbolType::array_begin;
	case ']': return SymbolType::array_end;
	case '(': return readString();
	case '%': return readComment();
	case '+':
	case '-': buff.push_back(static_cast<char>(i));
			  return readNumber();
	default:
		putBack(i);
		if (isdigit(i) || i == '.') return readNumber();
		if (isalpha(i)) return readKeyword();
		return SymbolType::unknown;
	}
}


template<typename Fn>
inline int SymbolStream<Fn>::readChar() {
	if (next < -1) return fn();
	else {
		int p = next; next = -2;return p;
	}

}

template<typename Fn>
inline int SymbolStream<Fn>::readSWS() {
	int i = readChar();
	while (i >= 0 && std::isspace(i)) i = readChar();
	return i;
}

inline int hex2num(char c) {
	return isdigit(c)?(c - '0'):c>'a'?(c-'a'+10):c>'A'?(c-'A'+10):0;
}

using KeywordDef = std::pair<std::string_view, SymbolType>;
extern std::vector<KeywordDef> keywords;

template<typename Fn>
inline Symbol SymbolStream<Fn>::readKeyword() {
	readToBuffer();
	auto iter = std::lower_bound(keywords.begin(), keywords.end(), KeywordDef(buff, SymbolType::not_set));
	if (iter == keywords.end() || iter->first != buff) return Symbol(SymbolType::unknown, buff);
	else return Symbol(iter->second);
}

template<typename Fn>
inline Symbol SymbolStream<Fn>::readComment() {
	int i = readChar();
	while (i != -1 && i != '\r' && i != '\n') {
		buff.push_back(static_cast<char>(i));
		i = readChar();
	}
	return Symbol(SymbolType::comment, buff);
}

template<typename Fn>
inline void SymbolStream<Fn>::readToBuffer() {
	int i = readChar();
	while (!isspace(i) && delimiters.find(i) != delimiters.npos ) {
		if (i=='#') {
			char a = static_cast<char>(readChar());
			char b = static_cast<char>(readChar());
			buff.push_back(static_cast<char>(hex2num(a)*16+hex2num(b)));
		} else {
			buff.push_back(static_cast<char>(i));
			i = readChar();
		}
	}
	putBack(i);
}
template<typename Fn>
inline Symbol SymbolStream<Fn>::readName() {
	readToBuffer();
	return Symbol(SymbolType::name, buff);
}


template<typename Fn>
inline Symbol SymbolStream<Fn>::readHex() {
	do {
		int i = readChar();
		if (i == '>') break;
		int j = readChar();
		if (i == '>') break;
		buff.push_back(static_cast<char>(hex2num(i)*16+hex2num(j)));
	} while (true);
	return Symbol(SymbolType::hex_string, buff);
}

template<typename Fn>
inline Symbol SymbolStream<Fn>::readString() {
	int pc = 1;
	do {
		int i = readChar();
		switch (i) {
			case -1: return SymbolType::unknown;
			case '(': ++pc;
					 buff.push_back('(');
					 break;
			case ')': --pc;
					 if (pc) buff.push_back(')');
					 break;
			case '\\': i = readChar();
					  switch(i) {
						  case 'n': buff.push_back('\n');break;
						  case 'r': buff.push_back('\r');break;
						  case 't': buff.push_back('\t');break;
						  case 'b': buff.push_back('\b');break;
						  case 'f': buff.push_back('\f');break;
						  case '(': buff.push_back('(');break;
						  case ')': buff.push_back(')');break;
						  case '\\': buff.push_back('\\');break;
						  default: if (i >= '0' && i < '8'){
							  int j = i-'0';
							  i = readChar();
							  if (i >= '0' && i < '8') {
								  j = j * 8 + (i - '0');
								  i = readChar();
								  if (i >= '0' && i < '8') {
									  j = j * 8 + (i - '0');
								  } else {
									  putBack(i);
								  }
							  } else {
								  putBack(i);
							  }
							  buff.push_back(static_cast<char>(j));
						  } else {
							  putBack(i);
						  }
					  }
					  break;
			default:
				buff.push_back(static_cast<char>(i));
		}
	} while (pc);
	return Symbol(SymbolType::string, buff);
}

template<typename Fn>
inline Symbol SymbolStream<Fn>::readNumber() {
	int i = readChar();
	while (std::isdigit(i)) {
		buff.push_back(static_cast<char>(i));
		i = readChar();
	}
	if (i == '.') {
		buff.push_back(static_cast<char>(i));
		int i = readChar();
		while (std::isdigit(i)) {
			buff.push_back(static_cast<char>(i));
			i = readChar();
		}
		putBack(i);
		return Symbol(std::strtod(buff.c_str(), nullptr));
	} else{
		putBack(i);
		return Symbol(static_cast<std::int64_t>(std::strtoll(buff.c_str(), nullptr, 10)));
	}
}


}


#endif /* SRC_PDF_PDF_LEX_H_ */

