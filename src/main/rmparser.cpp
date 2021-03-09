/*
 * rmparser.cpp
 *
 *  Created on: 7. 3. 2021
 *      Author: ondra
 */


#include "rmparser.h"

#include <cmath>
#include <iomanip>
#include <queue>

#include <imtjson/object.h>
json::NamedEnum<Drawing::Color> Drawing::strColor({
	{Color::black, "black"},
	{Color::white, "white"},
	{Color::gray, "gray"}
});

json::NamedEnum<Drawing::Brush> Drawing::strBrush({
	{Brush::BallPoint,"BallPoint"},
	{Brush::Marker,"Marker"},
	{Brush::Fineliner,"Fineliner"},
	{Brush::SharpPencil,"SharpPencil"},
	{Brush::TiltPencil,"TiltPencil"},
	{Brush::Brush,"Brush"},
	{Brush::Highlighter,"Highlighter"},
	{Brush::Eraser,"Eraser"},
	{Brush::EraseArea,"EraseArea"},
	{Brush::EraseAll,"EraseAll"},
	{Brush::Calligraphy,"Calligraphy"},
	{Brush::Pen,"Pen"},
	{Brush::SelectionBrush,"SelectionBrush"}
});


float Drawing::width_factor = 0.8f;

void swap32(void *what) {
	char *c = reinterpret_cast<char *>(what);
	std::swap(c[0],c[3]);
	std::swap(c[1],c[2]);
}

#include <cstdint>
void Drawing::Content::read(std::istream &in) {
	char buff[33];
	in.read(buff, sizeof(buff));
	if (!in || in.gcount() != sizeof(buff)) throw std::runtime_error("Header truncated");
	std::string_view hdrver(buff,sizeof(buff));

	if (hdrver == "reMarkable .lines file, version=3") version = 3;
	else if (hdrver == "reMarkable .lines file, version=5") version = 5;
	else throw std::runtime_error("Unsupported file version");

	if (version == 5) {
		char buff[10];
		in.read(buff, sizeof(buff));
		if (!in || in.gcount() != sizeof(buff)) throw std::runtime_error("Header truncated (version 5)");
		for (char c: buff) {
			if (c != 32) throw std::runtime_error("Unknown data in header");
		}
	}

	uint32_t layersCount;
	bool swap_bytes = false;
	in.read(reinterpret_cast<char *>(&layersCount),sizeof(layersCount));
	if (!in || in.gcount() != sizeof (layersCount))  throw std::runtime_error("File truncated (can't read layers)");
	if (layersCount > 16) {
		swap32(&layersCount);
		if (layersCount > 16) throw std::runtime_error("Invalid count of layers");
		swap_bytes = true;
	}


	Stream stream {
			version, swap_bytes, in
	};

	layers.clear();
	layers.reserve(layersCount);

	for (uint32_t i = 0; i < layersCount; i++) {
		Layer lr;
		lr.read(stream);
		layers.push_back(std::move(lr));
	}

}

void Drawing::Layer::read(const Stream &stream) {
	readArray(lines,  stream);
}

int Drawing::readInt32(const Stream &stream) {
	int32_t x;
	stream.stream.read(reinterpret_cast<char *>(&x), sizeof(x));
	if (!stream.stream || stream.stream.gcount() != sizeof(x)) throw std::runtime_error("Read error");
	if (stream.swap_endian) {
		swap32(&x);
	}
	return x;
}

json::Value Drawing::toJSON() const {
	using namespace json;

	Object ctx;
	ctx.set("version", content.version);
	ctx.set("layers", Value(json::array, content.layers.begin(),content.layers.end(), [&](const Layer &lr){
		return Object("lines", Value(json::array, lr.lines.begin(), lr.lines.end(), [&](const Line &ln){
			return Object("color",strColor[ln.color])
					     ("size",ln.size)
						 ("brush",strBrush[ln.type])
						 ("unknown1",ln.reserved1)
						 ("unknown2",ln.reserved2)
						 ("points", Value(json::array, ln.points.begin(), ln.points.end(), [&](const Point &pt){
				return Object("x", pt.x)
							 ("y", pt.y)
							 ("width", pt.width)
							 ("speed", pt.speed)
							 ("pressure", pt.pressure)
							 ("direction", pt.direction);
			}));
		}));
	}));
	return ctx;
}

float Drawing::readFloat32(const Stream &stream) {
	int32_t n = readInt32(stream);
	return *reinterpret_cast<float *>(&n);
}

template<typename T>
void Drawing::readArray(std::vector<T> &cont, const Stream &stream) {
	int entries = readInt32(stream);
	if (entries<0) throw std::runtime_error("Format error (invalid container size)");
	cont.clear();
	if (entries) {
		cont.reserve(entries);
		for (int i = 0; i < entries; i++) {
			T item;
			item.read(stream);
			cont.push_back(std::move(item));
		}
	}
}


void Drawing::Line::read(const Stream &stream) {
	type = readBrush(stream);
	color = readColor(stream);
	reserved1 = readInt32(stream);
	size = readFloat32(stream);
	if (stream.version >= 5) {
		reserved2 = readInt32(stream);
	} else {
		reserved2 = 0;
	}
	readArray(points, stream);
}

void Drawing::Point::read(const Stream &stream) {
	x = readFloat32(stream);
	y = readFloat32(stream);
	speed = readFloat32(stream);
	direction = readFloat32(stream);
	width = readFloat32(stream);
	pressure = readFloat32(stream);
}

Drawing::Color Drawing::readColor(const Stream &stream) {
	int c = readInt32(stream);
	switch (c) {
	case 0: return Color::black;
	case 1: return Color::gray;
	case 2: return Color::white;
	default: throw std::runtime_error("Format error - unknown colour: " + std::to_string(c));
	}
}

void Drawing::load_rm(std::istream &in) {
	content.read(in);
}

Drawing::Brush Drawing::readBrush(const Stream &stream) {
	int c = readInt32(stream);
	switch (c) {
	case 0:return Brush::Brush;
	case 1: return Brush::TiltPencil;
	case 2: return Brush::Pen;
	case 3: return Brush::Marker;
	case 4: return Brush::Fineliner;
	case 5: return Brush::Highlighter;
	case 6: return Brush::Eraser;
	case 7: return Brush::SharpPencil;
	case 8: return Brush::EraseArea;
	case 9: return Brush::EraseAll;
	case 10: return Brush::SelectionBrush;
	case 11: return Brush::SelectionBrush;
	case 12: return Brush::Brush;
	case 13: return Brush::SharpPencil;
	case 14: return Brush::TiltPencil;
	case 15: return Brush::BallPoint;
	case 16: return Brush::Marker;
	case 17: return Brush::Fineliner;
	case 18: return Brush::Highlighter;
	case 21: return Brush::Calligraphy;
	default: throw std::runtime_error("Format error - unknown brush type: "+ std::to_string(c));
	}

}





std::string Drawing::ColorDef::getColor(int layer, Brush brush, Color color) const {
	auto liter = layerColors.find(layer);
	if (liter != layerColors.end()) {
		auto biter = brushColors.find(brush);
		if (biter != brushColors.end()) {
			if (liter->second.priority > biter->second.priority) {
				return liter->second.chooseColor(color);
			} else {
				return biter->second.chooseColor(color);
			}
		} else {
			return liter->second.chooseColor(color);
		}
	} else {
		auto biter = brushColors.find(brush);
		if (biter != brushColors.end()) {
			return biter->second.chooseColor(color);
		} else {
			if (brush == Brush::Highlighter) {
				return "#f0dc28";
			}
			else {
				switch (color) {
				default:
				case Color::black: return "black";
				case Color::gray: return "gray";
				case Color::white: return "white";
				}
			}
		}
	}
}

const std::string& Drawing::OutColor::chooseColor(Color color) const {
	switch(color) {
	default:
	case Color::black: return black_color;
	case Color::gray: return gray_color;
	case Color::white: return white_color;
	}
}

static std::string putMaskAttr(int maskId) {
	if (maskId) {
		return "mask=\"url(#mask_" + std::to_string(maskId) + ")\" ";
	} else {
		return "";
	}
}

void Drawing::highlighter_to_svg(const Line &ln, const std::string &color, int maskId, std::ostream &out) {
	if (ln.points.empty()) return;
	const auto &first_point = ln.points[0];

	out << "<path fill=\"none\" "
		         "stroke-linecap=\"butt\" "
		         "stroke-linejoin=\"bevel\" "
		         "stroke-opacity=\"0.25\" "
		         "class=\"Highlighter\" "
  				  << putMaskAttr(maskId) <<
		         "stroke=\""<< color <<"\" "
		         "stroke-width=\"" << first_point.width << "\" "
		         "d=\"M " << first_point.x << " " << first_point.y;
	for (const auto &pt: ln.points) {
		out << " L " << pt.x << " " << pt.y;
	}
	out << "\" />";
}

const Drawing::Content& Drawing::getContent() const {
	return content;
}


void Drawing::fineliner_to_svg(const Line &ln, const std::string &color, int maskId, std::ostream &out) {
	if (ln.points.empty()) return;
	const auto &first_point = ln.points[0];

	out << "<path fill=\"none\" "
		         "stroke-linecap=\"round\" "
		         "class=\"Highlighter\" "
				 << putMaskAttr(maskId) <<
		         "stroke=\""<< color <<"\" "
		         "stroke-width=\"" << (first_point.width*width_factor) << "\" "
		         "d=\"M " << first_point.x << " " << first_point.y;
	for (const auto &pt: ln.points) {
		out << " L " << pt.x << " " << pt.y;
	}
	out << "\" />";
}

void Drawing::define_eraser_mask(const Line &ln, int id, std::ostream &out) {
	if (ln.points.empty()) return;
	const auto &first_point = ln.points[0];

	out << "<path id=\"eraser_" << id<< "\" "
			     "fill=\"none\" "
		         "stroke-linecap=\"round\" "
			  	  "stroke-linejoin=\"bevel\" "
		         "class=\"Eraser\" "
		         "stroke=\"black\" "
		         "stroke-width=\"" << (first_point.width) << "\" "
		         "d=\"M " << first_point.x << " " << first_point.y;
	for (const auto &pt: ln.points) {
		out << " L " << pt.x << " " << pt.y;
	}
	out << "\" />";

}

void Drawing::define_eraseArea_mask(const Line &ln, int id, std::ostream &out) {
	if (ln.points.empty()) return;
	const auto &first_point = ln.points[0];

	out << "<path id=\"eraser_" << id<< "\" "
			     "fill=\"black\" "
				  "stroke-linecap=\"round\" "
			      "stroke-linejoin=\"bevel\" "
				 "stroke-width=\"" << (first_point.width) << "\" "
		         "class=\"Eraser\" "
		         "stroke=\"black\" "
		         "d=\"M " << first_point.x << " " << first_point.y;
	for (const auto &pt: ln.points) {
		out << " L " << pt.x << " " << pt.y;
	}
	out << " Z";
	out << "\" />";

}

template<typename Cont>
void combineMasks(const Cont &mask_map, std::ostream &out) {
	for (const auto &m: mask_map) {
		out << "<use href=\"#eraser_" << m.second << "\" />";
	}

}

void Drawing::render_svg(std::ostream &out, const ColorDef &def) const {
	out << R"hdr(<?xml version="1.0" encoding="UTF-8"?>)hdr";
	out << "<svg viewBox=\"0 0 1404 1872\" xmlns=\"http://www.w3.org/2000/svg\">\r\n";
	out << "<def><rect id=\"viewport\" width=\"1404\" height=\"1872\" /></def>";
	out << R"flt(<filter id="blur"><feGaussianBlur in="SourceGraphic" stdDeviation="3" /></filter>)flt";
	int lrid = 1;
	int elem_id = 1;
	MaskQueue mask_map;

	auto updateMask = [&]{
			if (mask_map.empty()) return 0;
			int id = elem_id++;
			out << "<mask id=\"mask_" << id << "\">";
			out << "<use href=\"#viewport\" fill=\"white\" />";
			combineMasks(mask_map, out);
			out << "</mask>";
			return id;
	};

	for (const Layer &lr: content.layers) {
		out << "<g class=\"layer\">";
		mask_map.clear();
		out << "<def>";
		for (const Line &ln : lr.lines) {
			int id = elem_id++;
			switch (ln.type) {
				case Brush::Eraser: {
					define_eraser_mask(ln, id, out);
					mask_map.push_back({&ln,id});
				};break;
				case Brush::EraseArea: {
					define_eraseArea_mask(ln, id, out);
					mask_map.push_back({&ln,id});
				};break;
				default:break;
			}
		}
		out <<"</def>";
		int curMask = updateMask();
		for (const Line &ln : lr.lines) {
			if (!mask_map.empty() && mask_map.front().first == &ln) {
				mask_map.pop_front();
				curMask = updateMask();
			}
			if (!ln.points.empty()) {
				std::string color = def.getColor(lrid, ln.type, ln.color);
				switch (ln.type) {
				case Brush::Highlighter:highlighter_to_svg(ln, color, curMask, out);break;
				case Brush::Fineliner:fineliner_to_svg(ln, color, curMask, out);break;
				case Brush::BallPoint:
				case Brush::Brush:
				case Brush::Calligraphy:
				case Brush::Marker:
				case Brush::Pen:
				case Brush::SharpPencil:
				case Brush::TiltPencil:brush_to_svg(ln, color, curMask, mask_map, out);break;
				default:break;
				}
			}
		}
		out << "</g>\r\n";
		lrid++;
	}
	out << "</svg>";
}

void Drawing::brush_to_svg2(const Line &ln, const std::string &color, const std::string &maskAttr, std::ostream &out, bool opacity_to_color) {
	out<<"<g fill=\"none\" ";
	if (!color.empty()) out << "stroke=\"" << color << "\" ";
	out << "stroke-linecap=\"round\" "
			 << maskAttr <<
			"class=\"" << strBrush[ln.type] << "\">";
	auto iter = ln.points.begin();
	auto end = ln.points.end();
	float from_x= iter->x;
	float from_y = iter->y;
	++iter;
	while (iter != end) {
		float to_x = iter->x;
		float to_y = iter->y;

		float width;
		float opacity;
		switch (ln.type) {
		case Brush::BallPoint: width = iter->width;
							   opacity = std::pow(iter->pressure,5.0f)+0.7f;
							   break;
		case Brush::TiltPencil: width = iter->width;
							   opacity = std::pow(iter->pressure,1.0f);
							   break;

		default: width  = iter->width;
				 opacity = 1.0f;
		}
		out << "<path stroke-width=\"" << width * width_factor << "\" "
		    << "d=\"M " << from_x << " " << from_y << " L " << to_x << " " << to_y <<"\" ";
		if (opacity_to_color) {
			int col = std::min(255,static_cast<int>(opacity * 255.0));
			col = col | (col << 8) | col << 16;
			out << "stroke=\"#" << std::setfill('0') << std::setw(6) << std::hex << col << "\" " << std::dec;;
		}
		out << "/>";

		from_x= to_x;
		from_y= to_y;
		++iter;
	}
	out<<"</g>";

}

void Drawing::brush_to_svg(const Line &ln, const std::string &color, int maskId, const MaskQueue &mqueue, std::ostream &out) {
	std::string maskAttr;
	if (ln.type == Brush::BallPoint || ln.type == Brush::TiltPencil) {
		std::string commonId = std::to_string(reinterpret_cast<std::uintptr_t>(&ln));
		out << "<mask id=\"brush_" << commonId << "\">";
		out << "<use href=\"#viewport\" fill=\"black\" />";
		brush_to_svg2(ln,std::string(),"filter=\"url(#blur)\" ",out,true);
		combineMasks(mqueue, out);
		out << "</mask>";
		brush_to_svg2(ln,color,"mask=\"url(#brush_"+commonId+")\" ",out,false);
	} else {
		maskAttr = putMaskAttr(maskId);
		brush_to_svg2(ln,color,maskAttr,out, false);
	}

}

Drawing::Box Drawing::Box::merge(const Box &other) const {
	if (left > right) return other;
	if (other.left > other.right) return *this;
	return {
		std::min(left, other.left),
		std::min(top, other.top),
		std::max(right, other.right),
		std::max(bottom, other.bottom)
	};
}

void Drawing::Line::smoothLine(unsigned int cnt) {
	std::vector<Point> nwpt;
	while (points.size()>2 && cnt > 0) {
		std::size_t sz = points.size();
		nwpt.reserve(sz);
		nwpt.push_back(points[0]);
		for (std::size_t i = 2; i < sz-1; i++) {
			const auto &pt1 = points[i-1];
			const auto &pt2 = points[i];
			nwpt.push_back(Point{
				(pt1.x + pt2.x) *0.5f,
				(pt1.y + pt2.y) *0.5f,
				(pt1.speed + pt2.speed) *0.5f,
				(pt1.direction + pt2.direction) *0.5f,
				(pt1.width + pt2.width) *0.5f,
				(pt1.pressure + pt2.pressure) *0.5f
			});
		}
		nwpt.push_back(points.back());
		std::swap(nwpt, points);
		nwpt.clear();
		cnt--;
	}
}

Drawing::Box Drawing::Line::getBounds() const {
	if (points.empty()) return {1,1,0,0};
	Box bx {points[0].x,points[0].y, points[0].x, points[0].y};
	for (const auto &pt: points) {
		bx.left = std::min(bx.left, pt.x);
		bx.top= std::min(bx.top, pt.y);
		bx.right = std::max(bx.right, pt.x);
		bx.bottom= std::max(bx.bottom, pt.y);
	}
	return bx;
}

void Drawing::Layer::smoothLine(unsigned int cnt) {
	for (auto &l: lines) l.smoothLine(cnt);
}

Drawing::Box Drawing::Layer::getBounds() const {
	Box bx{1,1,0,0};
	for (const auto &ln: lines) {
		bx = bx.merge(ln.getBounds());
	}
	return bx;
}

void Drawing::Content::smoothLine(unsigned int cnt) {
	for (auto &l: layers) l.smoothLine(cnt);
}

Drawing::Box Drawing::Content::getBounds() const {
	Box bx{1,1,0,0};
	for (const auto &lr: layers) {
		bx = bx.merge(lr.getBounds());
	}
	return bx;
}

void Drawing::ColorDef::prepare() {
	layerColors.sort();
	brushColors.sort();
}
