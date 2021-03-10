/*
 * structs.cpp
 *
 *  Created on: 9. 3. 2021
 *      Author: ondra
 */

#include "structs.h"

#include <algorithm>
namespace pdf {

Element Dictionary::empty;

Element::Element():type(ElementType::nothing) {}
Element::Element(Dictionary &&dict):type(ElementType::dictionary),dict(std::move(dict)) {}

Element::Element(Array &&array):type(ElementType::array),array(std::move(array)) {}

Element::Element(const Element *ref):type(ElementType::reference),ref(ref) {}
Element::Element(Symbol &&symbol):type(ElementType::symbol),symbol(std::move(symbol)) {}

Element::Element(Stream &&stream):type(ElementType::stream),stream(std::move(stream)) {}

Element::Element(DecompStream &&stream):type(ElementType::decomp_stream),decomp_stream(std::move(stream)) {}

Element::~Element() {
	switch (type) {
	case ElementType::nothing:break;
	case ElementType::array: array.~Array();break;
	case ElementType::decomp_stream: decomp_stream.~DecompStream();break;
	case ElementType::stream: stream.~Stream();break;
	case ElementType::symbol: symbol.~Symbol();break;
	case ElementType::reference: break;
	case ElementType::dictionary: dict.~Dictionary();break;
	}
}

Element::Element(Element &&other):type(other.type) {
	other.type = ElementType::nothing;
	switch(type) {
	case ElementType::nothing:break;
	case ElementType::array: new(&array) Array(std::move(other.array));break;
	case ElementType::decomp_stream: new(&decomp_stream) DecompStream(std::move(other.decomp_stream));break;
	case ElementType::stream: new(&stream) Stream(std::move(other.stream));break;
	case ElementType::symbol: new(&symbol) Symbol(std::move(other.symbol));break;
	case ElementType::reference: ref = other.ref;break;
	case ElementType::dictionary:new(&dict) Dictionary(std::move(other.dict));break;
	}
}

const Element& Element::follow_ref() const {
	if (type == ElementType::reference) {
		if (ref == nullptr) return Dictionary::empty;
		else return ref->follow_ref();
	}
	else {
		return *this;
	}
}

Dictionary::Dictionary() {}

auto sort_keys() {
	return [](const auto &a, const auto &b) {
		return a.first < b.first;
	};
}

void Dictionary::sort() {
	std::sort(begin(), end(), sort_keys());
}

const Element& Dictionary::find(const std::string &what) const {
	auto iter = std::lower_bound(begin(), end(), std::pair(what, Element()) ,sort_keys());
	if (iter == end() ) return empty;
	return iter->second;
}

Array::Array() {}

const Element& Array::operator [](unsigned int pos) const {
	if (pos >= size()) return Dictionary::empty;
	const Element &el = Super::operator [](pos);
	return el;
}

}

