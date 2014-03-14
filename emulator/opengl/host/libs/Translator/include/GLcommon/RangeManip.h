/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#ifndef RANGE_H
#define RANGE_H

#include <set>

/**
 * Use std::pair to describe a Range [n1..n2]
 * std::pair provides relational operators
 * operators <, >, <= and >= perform a lexicographical
 * comparison on the sequence formed by members first and second
 */
class Range : public std::pair<int, int> {

public:
    Range():pair(){}
    Range(int start,int size):pair(start, start+size){}
    Range(const Range& r):pair(r){}
    void setRange(int start,int size) {pair::operator=(Range(start, size));}
    inline int getStart() const{return first;}
    inline int getEnd() const{return second;}
    inline int getSize() const{return second-first;}
    Range& operator=(const Range& r) {
        pair::operator=(r);
        return *this;
    }
    bool rangeIntersection(const Range& r,Range& rOut) const ;
    bool rangeUnion(const Range& r,Range& rOut) const ;
};

/**
 * Use std::set to store Range list
 * Sets are containers that store unique elements following a specific order.
 * The order is defined by the operator < () from std::pair
 */
class RangeList : public std::set<Range> {
public:
      void addRange(const Range& r) { insert(r); }
      void addRanges(const RangeList& rl) { insert(rl.begin(), rl.end()); }
      void delRange(const Range& r,RangeList& deleted);
      void delRanges(const RangeList& rl,RangeList& deleted);
      void merge();
};




#endif
