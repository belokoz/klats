#include "layout_pair.h"

namespace klats {

PairChoice choosePair(const std::vector<std::wstring>& chosen, const std::vector<const LayoutTable*>& layouts) {
    PairChoice choice;
    if (chosen.size() == 2 && chosen[0] != chosen[1]) {
        std::optional<size_t> first, second;
        for (size_t i = 0; i < layouts.size(); ++i) {
            if (!layouts[i] || layouts[i]->empty()) continue;
            if (layouts[i]->id() == chosen[0]) first = i;
            if (layouts[i]->id() == chosen[1]) second = i;
        }
        if (first && second) {
            choice.first = *first;
            choice.second = *second;
            return choice;
        }
    }

    std::vector<size_t> distinct;
    for (size_t i = 0; i < layouts.size(); ++i) {
        if (!layouts[i] || layouts[i]->empty()) continue;
        bool duplicate = false;
        for (size_t kept : distinct) duplicate = duplicate || layouts[kept]->forward() == layouts[i]->forward();
        if (!duplicate) distinct.push_back(i);
    }
    if (distinct.size() < 2) {
        choice.problem = PairChoice::Problem::NeedTwo;
    } else if (distinct.size() > 2) {
        choice.problem = PairChoice::Problem::Ambiguous;
    } else {
        choice.first = distinct[0];
        choice.second = distinct[1];
    }
    return choice;
}

}  // namespace klats
