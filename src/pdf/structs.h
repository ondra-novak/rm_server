/*
 * structs.h
 *
 *  Created on: 9. 3. 2021
 *      Author: ondra
 */

#ifndef SRC_PDF_STRUCTS_H_
#define SRC_PDF_STRUCTS_H_
#include <memory>

#include "pdf_lex.h"

namespace pdf {

enum class ElementType {

	nothing,
	dictionary, //dict,
	array, //array
	reference, //ref
	symbol,    //any symbol from Symbol (can be number, string, or anything else)
	stream,    //stream
	decomp_stream  //decompressed stream


};

class Element;
using PElement = std::unique_ptr<Element>;

class Dictionary : public std::vector<std::pair<std::string, Element> > {
public:
	Dictionary();
	using std::vector<std::pair<std::string, Element> >::vector;

	void sort();
	static Element empty;
	const Element &find(const std::string &what) const;

};
class Array: public std::vector<Element> {
public:
	typedef std::vector<Element> Super;
	Array();
	using std::vector<Element>::vector;
	const Element &operator[](unsigned int pos) const;
};

struct Stream {
	Dictionary dict;
	std::string_view stream;
};
struct DecompStream {
	Dictionary dict;
	std::string stream;
};

enum _TypeNumber {type_number};
enum _TypeString {type_string};

class Element {
public:




	Element();
	Element(Dictionary &&dict);
	Element(Array &&array);
	Element(const Element *ref);
	Element(Symbol &&symbol);
	Element(Stream &&stream);
	Element(DecompStream &&stream);
	~Element();
	Element(Element &&other);
	Element &operator=(Element &&other);

	const Array& getArray() const {return array;}
	const DecompStream& getDecompStream() const {return decomp_stream;}
	const Dictionary& getDict() const {return dict;}
	const Symbol& getSymbol() const {return symbol;}
	const Element* getRef() const {return ref;}
	const Stream& getStream() const {return stream;}
	ElementType getType() const {return type;}

	Array& getArray() {return array;}
	DecompStream& getDecompStream() {return decomp_stream;}
	Dictionary& getDict() {return dict;}
	Symbol& getSymbol() {return symbol;}
	Stream& getStream() {return stream;}


	//follow reference if any
	const Element &follow_ref() const;

protected:

	ElementType type;
	union {
		Dictionary dict;
		Array array;
		const Element *ref;
		Symbol symbol;
		Stream stream;
		DecompStream decomp_stream;
	};

};



}


#endif /* SRC_PDF_STRUCTS_H_ */
