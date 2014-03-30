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
#define OPPOSITE_FACE(x) (((x<<3) | (x>>3)) & 0x3f)
#define SHIFT_TO_FACE(x) (CircuitElement::shift_to_face[x])
#define FACE_TO_SHIFT(x) (CircuitElement::face_to_shift[x])
#define FACEDIR_TO_FACE(x) (CircuitElement::facedir_to_face[x])
#define WALLMOUNTED_TO_FACE(x) (CircuitElement::wallmounted_to_face[x])
#define DELTA_ANGLE_FACEDIR_TO_SHIFT(x, y) (CircuitElement::delta_angle_facedir_to_shift[x][y])
#define DELTA_ANGLE_WALLMOUNTED_TO_SHIFT(x, y) (CircuitElement::delta_angle_wallmounted_to_shift[x][y])

class CircuitElement;
class Circuit;
class GameScripting;
class Map;
class INodeDefManager;

struct MapNode;

enum FaceId
{
	FACE_BOTTOM = 0x1,
	FACE_BACK   = 0x2,
	FACE_LEFT   = 0x4,
	FACE_TOP    = 0x8,
	FACE_FRONT  = 0x10,
	FACE_RIGHT  = 0x20,
};

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
	CircuitElement(v3s16 pos, const unsigned char* func, unsigned long func_id, unsigned long id, unsigned int delay);
	CircuitElement(const CircuitElement& element);
	CircuitElement(unsigned long id);
	~CircuitElement();
	void addConnectedElement();
	void update();
	void updateState(GameScripting* m_script, Map& map, INodeDefManager* ndef);

	void serialize(std::ostream& out) const;
	void serializeState(std::ostream& out) const;
	void deSerialize(std::istream& is,
	                 std::map <unsigned long, std::list <CircuitElementVirtual>::iterator>& id_to_virtual_pointer);
	void deSerializeState(std::istream& is);

	void getNeighbors(std::vector <std::list <CircuitElementVirtual>::iterator>& neighbors) const;

	// First - pointer to object to which connected.
	// Second - face id.
	static void findConnectedWithFace(std::vector <std::pair <std::list<CircuitElement>::iterator, int > >& connected,
	                                  Map& map, INodeDefManager* ndef, v3s16 pos, FaceId face,
	                                  std::map<v3s16, std::list<CircuitElement>::iterator>& pos_to_iterator,
	                                  bool connected_faces[6]);
	// Get all faces that are connected to the "shift". Currently 6dfacedir and wallmounted params are not supported.
	static unsigned char getAcceptableFaces(const MapNode& node, const ContentFeatures& node_features, unsigned char shift);

	CircuitElementContainer getFace(int id) const;
	unsigned long getFuncId() const;
	v3s16 getPos() const;
	unsigned long getId() const;

	void connectFace(int id, std::list <CircuitElementVirtualContainer>::iterator it,
	                 std::list <CircuitElementVirtual>::iterator pt);
	void disconnectFace(int id);
	void setId(unsigned long id);
	void setInputState(unsigned char state);
	void setFunc(const unsigned char* func, unsigned long func_id);
	void setDelay(unsigned int delay);

	inline void addState(unsigned char state) {
		m_next_input_state |= state;
	}

	static unsigned char face_to_shift[33];
	static unsigned char opposite_shift[6];
	static FaceId shift_to_face[6];
	static FaceId facedir_to_face[6];
	static FaceId wallmounted_to_face[6];
	static unsigned char delta_angle_facedir_to_shift[4][6];
	static unsigned char delta_angle_wallmounted_to_shift[6][6];
private:
	v3s16 m_pos;
	unsigned long m_element_id;
	const unsigned char* m_func;
	unsigned long m_func_id;
	unsigned char m_current_input_state;
	unsigned char m_next_input_state;
	unsigned char m_current_output_state;
	unsigned char m_next_output_state;
	std::deque <unsigned char> m_states_queue;
	CircuitElementContainer m_faces[6];
};

#endif
