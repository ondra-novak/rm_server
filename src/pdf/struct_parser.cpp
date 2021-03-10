/*
 * struct_parser.cpp
 *
 *  Created on: 9. 3. 2021
 *      Author: ondra
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "struct_parser.h"

#include <cerrno>
#include <system_error>
namespace pdf {

static std::string_view mapFile(const std::string &fname) {
	int fd = ::open(fname.c_str(), O_RDONLY);
	if (fd<0) throw std::system_error(errno, std::generic_category(), fname);
	auto len = ::lseek(fd, 0, SEEK_END);
	void *p = mmap(0, len, PROT_READ,MAP_SHARED,fd,0);
	if (p == nullptr) {
		int e = errno;
		::close(fd);
		throw std::system_error(e, std::generic_category(), fname);
	}
	::close(fd);
	return std::string_view(static_cast<const char *>(p), len);
}

MappedFile::MappedFile(const std::string &fname):std::string_view(mapFile(fname)) {}

MappedFile::~MappedFile() {
	munmap(const_cast<char *>(data()),length());
}

void PDFFile::parseValue(SymbReader sstream, Stack &elstk) {
	bool cont = true;
	do {
		Symbol symb = sstream.read();
		switch (symb.type) {
		case SymbolType::comment:
			break;
		case SymbolType::endobj:
			cont = false;
			break;
		case SymbolType::name:
		case SymbolType::string:
		case SymbolType::hex_string:
		case SymbolType::number:
		case SymbolType::int_number:
		case SymbolType::array_begin:
		case SymbolType::bool_true:
		case SymbolType::bool_false:
		case SymbolType::null:
		case SymbolType::dict_begin:
			elstk.push(std::move(symb));
			break;
		case SymbolType::dict_end:
			elstk.push(makeDictionary(elstk));
			break;
		case SymbolType::array_end:
			elstk.push(makeArray(elstk));
			break;
		case SymbolType::stream:
			elstk.push(makeStream(elstk, sstream));
			break;
		case SymbolType::reference_mark:
			elstk.push(makeReference(elstk));
			break;
		default:
			throw std::runtime_error("Corrupted format - unexpected sequence");
		}
	} while (cont);
	if (elstk.size() != 1)
		throw std::runtime_error("Corrupted format - unfinished structure");
}

const Element &PDFFile::getObject(ObjID  id) {
	Stack elstk;

	auto iter = inv.find(id);
	if (iter == inv.end()) throw std::runtime_error("Object not found");
	if (iter->second.ref != nullptr) return *iter->second.ref;


	SymbReader sstream = readFrom(iter->second.offset);
	do {
		Symbol symb = sstream.read();
		if (symb.isSymbol(SymbolType::obj)) break;
		else if (symb.isSymbol(SymbolType::number))  elstk.push(std::move(symb));
		else throw std::runtime_error("Corrupted format - expected begin of object");
	} while (true);

	if (elstk.size() <2) throw std::runtime_error("Corrupted format - expected two numbers before obj");
	//auto gen = elstk.top().getSymbol().getInt(); don't read gen
	elstk.pop();
	auto objid = elstk.top().getSymbol().getInt();
	if (objid != id) throw std::runtime_error("Corrupted format - probably corrupted xref - unexpected object");
	elstk.pop();

	parseValue(sstream, elstk);
	Element el = std::move(elstk.top());
	iter->second.ref = std::make_unique<Element>(std::move(el));
	return *iter->second.ref;
}

Element PDFFile::makeDictionary(Stack &stack) {
	Dictionary dict;
	while (!stack.empty()) {
		Element el(std::move(stack.top()));stack.pop();
		if (el.getType() == ElementType::symbol && el.getSymbol().type == SymbolType::dict_end) {
			dict.sort();
			return dict;
		}
		if (stack.empty()) throw std::runtime_error("Corrupted format - dictionary is not complete");
		Element key(std::move(stack.top()));stack.pop();
		if (key.getType() == ElementType::symbol && key.getSymbol().type == SymbolType::name) {
			dict.emplace_back(std::move(key.getSymbol().text), std::move(el));
		} else {
			throw std::runtime_error("Corrupted format - dictionary key must be a name");
		}
	}
	throw std::runtime_error("Corrupted format - dictionary was not open");
}

Element PDFFile::makeArray(Stack &stack) {
	Array arr;
	while (!stack.empty()) {
		Element el(std::move(stack.top()));stack.pop();
		if (el.getType() == ElementType::symbol && el.getSymbol().type == SymbolType::array_end) {
			return arr;
		}
		arr.push_back(std::move(el));
	}
	throw std::runtime_error("Corrupted format - dictionary was not open");
}

Element PDFFile::makeStream(Stack &stack, SymbReader &symbstream) {
	if (stack.empty()) throw std::runtime_error("Corrupted format - stream without dictionary");
	Element el(std::move(stack.top()));stack.pop();
	if (el.getType() != ElementType::dictionary) throw std::runtime_error("Corrupted format - only dictionary is allowed before stream");
	int c = symbstream.readChar();
	while (c != 10 && c != -1) c = symbstream.readChar();
	if (c == 10) {
		const Element &ellen = el.getDict().find("Length").follow_ref();
		if (ellen.getType() == ElementType::symbol && ellen.getSymbol().isSymbol(SymbolType::number)) {
			auto len = ellen.getSymbol().getInt();
			auto data = symbstream.getStream().skipView(len);
			if (data.size() == static_cast<std::size_t>(len)) {
				Symbol send = symbstream.read();
				if (send.type == SymbolType::endstream) {
					return Element(Stream{std::move(el.getDict()),data});
				} else {
					throw std::runtime_error("Missing endstream");
				}
			} else {
				throw std::runtime_error("Unexpected end of file when reading stream");
			}
		} else {
			throw std::runtime_error("Length required");
		}
	}
	throw std::runtime_error("Unexpected end of file when reading stream");

}

PDFFile::PDFFile(const std::string_view &data):data(data) {
}

void PDFFile::init() {
	auto pos = data.rfind("startxref");
	if (pos == data.npos) throw std::runtime_error("Can't find startxref");
	pos+=9;
	SymbReader rd = readFrom(pos);
	Symbol smb = rd.read();
	if (smb.isSymbol(SymbolType::number)) {
		std::size_t xrefofs = smb.getInt();

		SymbReader xrefrd = readFrom(xrefofs);
		if (xrefrd.read().type != SymbolType::xref) throw std::runtime_error("Can't find xref");
		do {
			auto start = xrefrd.read();
			if (start.type == SymbolType::trailer) break;
			if (start.type != SymbolType::int_number) throw std::runtime_error("Corrupted xref (start)");
			auto count = xrefrd.read();
			if (count.type != SymbolType::int_number) throw std::runtime_error("Corrupted xref (count)");
			for (unsigned int i = 0, s = start.getInt(), cnt = count.getInt(); i < cnt; i++) {
				auto offset = xrefrd.read();
				if (offset.type != SymbolType::int_number) throw std::runtime_error("Corrupted xref (offset)");
				auto gen = xrefrd.read();
				if (gen.type != SymbolType::int_number) throw std::runtime_error("Corrupted xref (gen)");
				auto st = xrefrd.read();
				if (st.type != SymbolType::free_obj && st.type != SymbolType::used_obj) throw std::runtime_error("Corrupted xref (f/n)");
				inv.emplace(s+i, InventoryItem{static_cast<std::size_t>(offset.getInt()), static_cast<unsigned int>(gen.getInt()), st.type == SymbolType::free_obj, nullptr});
			}
		} while (true);
		trailer_ofs = xrefrd.getStream().pos;
		{
		}
	} else {
		throw std::runtime_error("Can't read startxref");
	}



}

const Element &PDFFile::getCatalog() {
	Stack stack;
	SymbReader xrefrd = readFrom(trailer_ofs);
	parseValue(xrefrd, stack);
	if (stack.size() != 1 || stack.top().getType() != ElementType::dictionary) throw std::runtime_error("Corrupted trailer");
	trailer_data = std::move(stack.top().getDict());
	const Element &el = trailer_data.find("Root").follow_ref();
	return el;
}

Element PDFFile::makeReference(Stack &stack) {
	if (stack.size() <2) throw std::runtime_error("Corrupted format - expected two numbers before R");
	//auto gen = elstk.top().getSymbol().getInt(); don't read gen
	stack.pop();
	auto objid = stack.top().getSymbol().getInt();
	stack.pop();
	return Element(&getObject(objid));

}

std::string_view PDFFile::Reader::skipView(std::size_t len) {
	if (len +pos > data.size()) len = data.size() - pos;
	auto sub = data.substr(pos,len);
	pos+=len;
	return sub;
}

int PDFFile::Reader::operator ()() {
	if (pos >= data.size()) return -1;
	else return data[pos++];

}


}
