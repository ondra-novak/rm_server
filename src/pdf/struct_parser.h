/*
 * struct_parser.h
 *
 *  Created on: 9. 3. 2021
 *      Author: ondra
 */

#ifndef SRC_PDF_STRUCT_PARSER_H_
#define SRC_PDF_STRUCT_PARSER_H_

#include <map>
#include <stack>

#include <string_view>
#include "structs.h"


namespace pdf {


class MappedFile: public std::string_view {
public:

	MappedFile(const std::string &fname);
	~MappedFile();

};

class PDFFile {
public:

	using ObjID = unsigned int;

	PDFFile(const std::string_view &data);

	///initialize struct, search for xref and parse it - doesn't load objects
	void init();
	///retrieves catalog - at this point, parsing is done
	const Element &getCatalog();

	///retrieve object from xref inventory - if not parsed yet, parsing is done now
	const Element & getObject(ObjID id);


	struct InventoryItem {
		///offset in file - if known - offset is zero for newly added objects
		std::size_t offset;
		///object generation - if known
		unsigned int generation;
		///true if item is free
		bool is_free;
		///pointer to element - it could be newly added, when offset is zero
		std::unique_ptr<Element> ref;
	};


	struct Reader {
		std::string_view data;
		std::size_t pos;
		std::string_view skipView(std::size_t len);
		int operator()();
	};

	using SymbReader = SymbolStream<Reader>;

	SymbReader readFrom(std::size_t offset) {
		return SymbReader(Reader{data, offset});
	}

	std::size_t getObjectOffset(ObjID object) const;

	using Inventory = std::map<ObjID, InventoryItem>;

protected:
	std::string_view data;
	Inventory inv;
	std::size_t trailer_ofs;
	Dictionary trailer_data;

	using Stack = std::stack<Element>;


	Element makeDictionary(Stack &stack);
	Element makeArray(Stack &stack);
	Element makeStream(Stack &stack, SymbReader &symbstream);
	Element makeReference(Stack &stack);
	void parseValue(SymbReader sstream, Stack &elstk);
};


}



#endif /* SRC_PDF_STRUCT_PARSER_H_ */
