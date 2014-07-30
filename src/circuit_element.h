#ifndef CIRCUIT_ELEMENT_H
#define CIRCUIT_ELEMENT_H

#include "irr_v3d.h"
#include "mapnode.h"
#include "circuit_element_virtual.h"
#include "nodedef.h"

#include <list>
#include <vector>
#include <map>
#include <deque>

#define OPPOSITE_SHIFT(x) (CircuitElement::opposite_shift[x])
#define OPPOSITE_FACE(x) (CircuitElement::opposite_face[x])
#define SHIFT_TO_FACE(x) (1 << x)
#define FACE_TO_SHIFT(x) (CircuitElement::face_to_shift[x])
#define ROTATE_FACE(x, y) (CircuitElement::rotate_face[FACE_TO_SHIFT(x) * 24 + y])                 // Rotate real face
#define REVERSE_ROTATE_FACE(x, y) (CircuitElement::reverse_rotate_face[FACE_TO_SHIFT(x) * 24 + y]) // Get real face of node
#define ROTATE_SHIFT(x, y) (FACE_TO_SHIFT(CircuitElement::rotate_face[x * 24 + y]))
#define REVERSE_ROTATE_SHIIFT(x, y) (FACE_TO_SHIFT(CircuitElement::reverse_rotate_face[x * 24 + y]))

class CircuitElement;
class Circuit;
class GameScripting;
class Map;
class INodeDefManager;

// enum FaceId
// {
// 	FACE_TOP    = 0x1,
// 	FACE_BOTTOM = 0x2,
// 	FACE_RIGHT  = 0x4,
// 	FACE_LEFT   = 0x8,
// 	FACE_BACK   = 0x10,
// 	FACE_FRONT  = 0x20
// };

/*
 * Graph example:
 * E   E   E
 *  \ / \ /
 *   V   V
 *  / \
 * E   E
 *
 * E - normal elements
 * V - virtual elements
 */

struct CircuitElementContainer
{
	std::list <CircuitElementVirtualContainer>::iterator list_iterator;
	std::list <CircuitElementVirtual>::iterator list_pointer;

	bool is_connected;
};

class CircuitElement {
public:
	CircuitElement(v3s16 pos, u32 id, u8 delay);
	CircuitElement(const CircuitElement& element);
	CircuitElement(u32 id);
	~CircuitElement();
	void addConnectedElement();
	void update();
	bool updateState(GameScripting* m_script, Map* map, INodeDefManager* ndef);
	void resetState();

	void serialize(std::ostream& out) const;
	void serializeState(std::ostream& out) const;
	void deSerialize(std::istream& is,
	                 std::map <u32, std::list <CircuitElementVirtual>::iterator>& id_to_virtual_pointer);
	void deSerializeState(std::istream& is);

	void getNeighbors(std::vector <std::list <CircuitElementVirtual>::iterator>& neighbors) const;

	// First - pointer to object to which connected.
	// Second - face id.
	static void findConnectedWithFace(std::vector <std::pair <std::list<CircuitElement>::iterator, u8> >& connected,
	                                  Map* map, INodeDefManager* ndef, v3s16 pos, u8 face,
	                                  std::map<v3s16, std::list<CircuitElement>::iterator>& pos_to_iterator,
	                                  bool connected_faces[6]);

	CircuitElementContainer getFace(int id) const;
	v3s16 getPos() const;
	u32 getId() const;

	void connectFace(int id, std::list <CircuitElementVirtualContainer>::iterator it,
	                 std::list <CircuitElementVirtual>::iterator pt);
	void disconnectFace(int id);
	void setId(u32 id);
	void setInputState(u8 state);
	void setDelay(u8 delay);

	void swap(const MapNode& n_old, const ContentFeatures& n_old_features,
	          const MapNode& n_new, const ContentFeatures& n_new_features);

	inline void addState(u8 state) {
		m_next_input_state |= state;
	}

	inline static u8 rotateFace(const MapNode& node, const ContentFeatures& node_features, u8 face) {
		if(node_features.param_type_2 == CPT2_FACEDIR) {
			return ROTATE_FACE(face, node.param2);
		} else {
			return face;
		}
	}

	inline static u8 revRotateFace(const MapNode& node, const ContentFeatures& node_features, u8 face) {
		if(node_features.param_type_2 == CPT2_FACEDIR) {
			return REVERSE_ROTATE_FACE(face, node.param2);
		} else {
			return face;
		}
	}

	static u8 face_to_shift[33];
	static u8 opposite_shift[6];
	static u8 opposite_face[33];
	static u8 shift_to_face[6];
	static u8 rotate_face[168];
	static u8 reverse_rotate_face[168];
private:
	v3s16 m_pos;
	u32 m_element_id;
	u8 m_prev_input_state;
	u8 m_current_input_state;
	u8 m_next_input_state;
	u8 m_current_output_state;
	std::deque <u8> m_states_queue;
	CircuitElementContainer m_faces[6];
};

#endif
