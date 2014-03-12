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
#include <GLcommon/RangeManip.h>
#include <stdio.h>


bool Range::rangeIntersection(const Range& r,Range& rOut) const {
    if(getStart() > r.getEnd() || r.getStart() > getEnd()) return false;
    int max_start = (getStart() > r.getStart())? getStart():r.getStart();
    int min_end = (getEnd() < r.getEnd())? getEnd():r.getEnd();
    int size = min_end - max_start;
    if(size) {
        rOut.setRange(max_start,min_end-max_start);
        return true;
    }
    return false;
}

bool Range::rangeUnion(const Range& r,Range& rOut) const {
    if(getStart() > r.getEnd() || r.getStart() > getEnd()) return false;
    int min_start = (getStart() < r.getStart())?getStart():r.getStart();
    int max_end = (getEnd() > r.getEnd())?getEnd():r.getEnd();
    int size =  max_end - min_start;
    if(size) {
        rOut.setRange(min_start,max_end-min_start);
        return false;
    }
    return false;
}

void RangeList::delRanges(const RangeList& rl,RangeList& deleted) {

    for(iterator it = rl.begin(); it != rl.end(); ++it) {
        delRange(*it,deleted);
    }

}

void RangeList::delRange(const Range& r,RangeList& deleted) {
    if(r.getSize() == 0) return;

    Range intersection;
    RangeList newList;

    iterator it=lower_bound(r);

    // if there is intersection
    for(iterator it = begin(); it != end(); ++it) {

        if(r.rangeIntersection(*it, intersection)) {

            if (intersection!=*it) { // otherwise split:
                //intersection on right side
                if(it->getStart() != intersection.getStart()) {
                    newList.insert(it,Range(it->getStart(),intersection.getStart() - it->getStart()));
                }

                //intersection on left side
                if(it->getEnd() != intersection.getEnd()) {
                    newList.insert(it,Range(intersection.getEnd(),it->getEnd() - intersection.getEnd()));
                }
            }
            deleted.addRange(intersection);
        } else {
            newList.insert(newList.end(), *it);
        }
    }

    operator=(newList);
}

void RangeList::merge() {
    if(empty()) return;

    RangeList newList;
    RangeList::iterator it = begin();
    Range current = *it;
    it++;

    while(it != end()) {
        if(current.getEnd() >= it->getStart()) {
            current.second = std::max(current.second, it->second);
        } else {
            newList.insert(newList.end(), current);
            current = *it;
        }
        it++;
    }
    newList.insert(newList.end(), current);

    operator=(newList);
}
