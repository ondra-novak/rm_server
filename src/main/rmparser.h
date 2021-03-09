/*
 * rmparser.h
 *
 *  Created on: 7. 3. 2021
 *      Author: ondra
 */

#ifndef SRC_MAIN_RMPARSER_H_
#define SRC_MAIN_RMPARSER_H_

#include <deque>
#include <iostream>
#include <vector>

#include <imtjson/value.h>
#include <imtjson/namedEnum.h>
#include "sorted_vector.h"
class Drawing {
public:

	enum class Brush {
	    BallPoint,
	    Marker,
	    Fineliner,
	    SharpPencil,
	    TiltPencil,
	    Brush,
	    Highlighter,
	    Eraser,
	    EraseArea,
	    EraseAll,
	    Calligraphy,
	    Pen,
	    SelectionBrush
	};

	enum class Color {
		black = 0, gray = 1, white = 2
	};


	struct Box {
		float left,top,right,bottom;
		Box merge(const Box &other) const;
	};

	struct Stream {
		int version;
		bool swap_endian;
		std::istream &stream;

	};

	struct Point {
		float x;
		float y;
		float speed;
		float direction;
		float width;
		float pressure;

		void read(const Stream &stream);
	};

	struct Line {
		Brush type;
		Color color;
		float size;
		int reserved1;
		int reserved2;
		std::vector<Point> points;

		void read(const Stream &stream);
		void smoothLine(unsigned int cnt);
		Box getBounds() const;

	};

	struct Layer {
		std::vector<Line> lines;

		void read(const Stream &stream);
		void smoothLine(unsigned int cnt);
		Box getBounds() const;
	};

	struct Content {
		int version;
		std::vector<Layer> layers;

		void read(std::istream &in);
		void smoothLine(unsigned int cnt);
		Box getBounds() const;
	};


	struct OutColor {
		std::string black_color, gray_color, white_color;
		float priority;

		const std::string &chooseColor(Color color) const;
	};


	struct ColorDef {
		SortedVector<int, OutColor> layerColors;
		SortedVector<Brush, OutColor> brushColors;

		std::string getColor(int layer, Brush brush, Color color) const;
		void prepare();
	};

	const Content &getContent() const;


	json::Value toJSON() const;
	void render_svg(std::ostream &out, const ColorDef &def) const;

	static json::NamedEnum<Color> strColor;
	static json::NamedEnum<Brush> strBrush;

	void load_rm(std::istream &in);
	void smooth(unsigned int cnt) {content.smoothLine(cnt);}


protected:

	Content content;
	using MaskQueue = std::deque<std::pair<const Line *, int> >;


	static int readInt32(const Stream &stream);
	static float readFloat32(const Stream &stream);
	template<typename T>
	static void readArray(std::vector<T> &cont, const Stream &stream);
	static Color readColor(const Stream &stream);
	static Brush readBrush(const Stream &stream);

	static void highlighter_to_svg(const Line &ln, const std::string &color, int maskId, std::ostream &out);
	static void fineliner_to_svg(const Line &ln, const std::string &color, int maskId, std::ostream &out);
	static void brush_to_svg(const Line &ln, const std::string &color, int maskId, const MaskQueue &mqueue, std::ostream &out);
	static void brush_to_svg2(const Line &ln, const std::string &color, const std::string &maskAttr, std::ostream &out, bool opacity_to_color);
	static void define_eraser_mask(const Line &ln, int id, std::ostream &out);
	static void define_eraseArea_mask(const Line &ln, int id, std::ostream &out);

	static float width_factor;

};




#endif /* SRC_MAIN_RMPARSER_H_ */
