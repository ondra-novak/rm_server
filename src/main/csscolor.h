/*
 * csscolor.h
 *
 *  Created on: 7. 3. 2021
 *      Author: ondra
 */

#ifndef SRC_MAIN_CSSCOLOR_H_
#define SRC_MAIN_CSSCOLOR_H_

#include <string_view>

class CSSColor {
public:
	CSSColor(const std::string_view &cssname);
	CSSColor(float r, float g, float b, float a);

	void setColor(const std::string_view &cssname);
	std::string getCSSColor() const;
	CSSColor mix(const CSSColor &with) const;

	float r, g, b, a;

};

#endif /* SRC_MAIN_CSSCOLOR_H_ */
