/** 
 * @file MapVector.h
 *  This file defines a MapVector class that combines the 
 *  powers of std::map and std::vector for forward mapping
 *  objects of arbitrary types to integers and also 
 *  reverse mapping.
 **/

#ifndef MAPVECTOR_H
#define MAPVECTOR_H

#include<map>
#include<vector>

/**
 * @struct MapVector
 * @brief 
 *  Class that combines std::map with std::vector for integer indexing
 *  of objects and also reverse mapping.
 * 
 * @tparam  T  Any valid type for which std::map can be created
 * 
 * @details
 *  Maps values of parameter type T to integers at the same time 
 *  adding the value to a std::vector for reverse mapping. The class 
 *  also provides various methods for ease of working with the objects.
 **/
template<typename T>
class MapVector {
public:
    // these two declarations are required because std::iterator is 
    // now deprecated
    using value_type = T;
    using reference = T&;

    // declarations for better readability
    using const_reference = const T&;
    using map_type = typename std::map<value_type, int>;
    using vector_type = typename std::vector<value_type>;
    using iterator = typename vector_type::const_iterator;
    using const_iterator = typename vector_type::const_iterator;
    using reverse_iterator = typename vector_type::const_reverse_iterator;
    using const_reverse_iterator = typename vector_type::const_reverse_iterator;
    using size_type = typename vector_type::size_type;

    // Methods to return iterators for the class. These are basically 
    // the corresponding iterators to the underlying Vector data member 
    // of the class
    iterator begin() { return Vector.begin(); }
    const_iterator begin () const { return Vector.begin(); }
    reverse_iterator rbegin() { return Vector.rbegin(); }
    const_reverse_iterator rbegin () const { return Vector.rbegin(); }
    iterator end() { return Vector.end(); }
    const_iterator end () const { return Vector.end(); }
    reverse_iterator rend() { return Vector.rend(); }
    const_reverse_iterator rend () const { return Vector.rend(); }

    /** 
     * @brief   Constructor for MapVector class.
     **/
    MapVector() : Map({}), Vector({}) {}

    /**
     * @brief Method to return whether the object is empty.
     * 
     * @returns     True if the object does not contain any value
     *              otherwise false
     **/
    bool empty() { return Vector.empty(); }

    /**
     * @brief 
     *  Method to return the size of the object - the number of 
     *  distinct values added to the object.
     * 
     * @returns     The number of distinct values added to the 
     *              object
     **/
    size_type size() { return Vector.size(); }

    /**
     * @brief Method used for forward mapping.
     * 
     * @param   el  Value of template type T to be forward mapped
     * 
     * @returns     The index corresponding to the value if it is 
     *              already present in Map otherwise -1
     **/
    int getInt(const_reference el) const {
        auto it = Map.find(el);

        if(it == Map.end()) return -1;
        return it->second;
    }

    /**
     * @brief
     *  The operator is overloaded for reverse mapping. The method 
     *  checks for range overflows, raising exception.
     * 
     * @param   n   The index to be reverse mapped
     * 
     * @returns     The value corresponding to the index
     **/
    const_reference operator[](size_type n) const {
        assert(n < Vector.size() && "MapVector access out of range");
        return Vector[n];
    }

    /** 
     * @brief Method to insert a new value.
     * 
     * @param   el  The value to be inserted
     * 
     * @returns     A std::pair object whose second element is a boolean
     *              indicating whether the value was inserted or not (if
     *              it was already inserted before) and the index to which
     *              it has been mapped
     **/
    std::pair<int, bool> insert(const_reference el) {
        int idx = Vector.size();
        std::pair<typename map_type::iterator, bool> ret = Map.insert({el, idx});

        if(ret.second) {
            Vector.push_back(el);
            return {idx, true};
        }
        return {ret.first->second, false};
    }

    /** 
     * @brief Method to clear the object.
     * 
     * @returns     None
     **/
    void clear() { Map.clear(), Vector.clear(); }

private:
    /**
     * @brief   std::map object for forward integer mapping.
     **/
    map_type Map;

    /**
     * @brief   std::vector object for reverse mapping.
     **/
    vector_type Vector;
};

#endif