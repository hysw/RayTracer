#include "inc/enums.h"
#include "inc/constants.h"
#include "inc/entity.h"
#include "inc/record.h"
#include <iostream>

HYSW_RAYTRACER_NAMESPACE_START


int SerializationMap::serialize(std::vector<char>& buf, Entity* e) {
	typedef std::map<Entity*, int>::iterator iter_t;
	SerializationMap m;
	m.add(e);
	buf.resize(m.compute_serialization(buf.size()));
	for(iter_t i = m.entities.begin();i!=m.entities.end();++i) {
		i->first->write(m, &buf[i->second]);
	}
	return m.at(e);
}

SerializationMap& SerializationMap::add(Entity* e) {
	if(e && entities.count(e) == 0) {
		entities[e] = 0;
		e->inject(*this);
	}
	return *this;
}

int SerializationMap::at(Entity* e) const {
	typedef std::map<Entity*, int>::const_iterator iter_t;
	iter_t iter = entities.find(e);
	return iter!=entities.end()?iter->second:0;
}

int SerializationMap::compute_serialization(int base) {
	typedef std::map<Entity*, int>::iterator iter_t;
	for(iter_t i = entities.begin();i!=entities.end();++i) {
		i->second = base;
		base += i->first->size();
	}
	return base;
}

HYSW_RAYTRACER_NAMESPACE_END