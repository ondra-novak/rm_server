/*
 * sorted_vector.h
 *
 *  Created on: 7. 3. 2021
 *      Author: ondra
 */

#ifndef SRC_MAIN_SORTED_VECTOR_H_
#define SRC_MAIN_SORTED_VECTOR_H_

#include <cstddef>

template<typename K, typename V, typename Cmp = std::less<K> >
class SortedVector: public std::vector<std::pair<K, V> > {
public:
	using value_type = std::pair<K, V>;
	using Super = std::vector<value_type>;

	SortedVector() {}
	SortedVector(Cmp &&cmp):cmp(std::move(cmp)) {}
	using std::vector<std::pair<K, V> >::vector;

	void sort();
	typename Super::const_iterator find(const K &item) const;


protected:
	Cmp cmp;
};

template<typename K, typename V, typename Cmp>
inline void SortedVector<K, V, Cmp>::sort() {
	std::sort(this->begin(), this->end(), [&](const auto &a, const auto &b) {
		return this->cmp(a.first, b.first);
	});
}

template<typename K, typename V, typename Cmp>
inline typename std::vector<std::pair<K, V> >::const_iterator SortedVector<K, V,Cmp>::find(const K &item) const {
	auto ofs = offsetof(value_type, first);
	const std::pair<K, V> *srch = reinterpret_cast<const std::pair<K, V> *>(
			reinterpret_cast<const char *>(&item) -  ofs
	);
	auto iter = std::lower_bound(this->begin(), this->end(), *srch, [&](const auto &a, const auto &b){
		return this->cmp(a.first, b.first);
	});
	if (iter != this->end() && iter->first == item) return iter;
	else return this->end();
}

#endif /* SRC_MAIN_SORTED_VECTOR_H_ */
