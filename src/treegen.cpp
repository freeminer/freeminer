/*
treegen.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>,
Copyright (C) 2012-2013 RealBadAngel, Maciej Kasatkin <mk@realbadangel.pl>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "irr_v3d.h"
#include <stack>
#include "util/pointer.h"
#include "util/numeric.h"
#include "util/mathconstants.h"
#include "map.h"
#include "environment.h"
#include "nodedef.h"
#include "treegen.h"

#include "log.h"

namespace treegen
{

void make_tree(ManualMapVoxelManipulator &vmanip, v3s16 p0,
		bool is_apple_tree, INodeDefManager *ndef, int seed)
{
	/*
		NOTE: Tree-placing code is currently duplicated in the engine
		and in games that have saplings; both are deprecated but not
		replaced yet
	*/
	MapNode treenode(ndef->getId("mapgen_tree"));
	MapNode leavesnode(ndef->getId("mapgen_leaves"));
	MapNode applenode(ndef->getId("mapgen_apple"));

	PseudoRandom pr(seed);
	s16 trunk_h = pr.range(4, 6);
	v3s16 p1 = p0;
	for(s16 ii=0; ii<trunk_h; ii++)
	{
		if(vmanip.m_area.contains(p1))
			if(ii == 0 || vmanip.getNodeNoExNoEmerge(p1).getContent() == CONTENT_AIR)
				vmanip.m_data[vmanip.m_area.index(p1)] = treenode;
		p1.Y++;
	}

	// p1 is now the last piece of the trunk
	p1.Y -= 1;

	s16 size = pr.range(2, 3);
	VoxelArea leaves_a(v3s16(-size,-pr.range(2, 3),-size), v3s16(size,pr.range(2, 3),size));
	//SharedPtr<u8> leaves_d(new u8[leaves_a.getVolume()]);
	Buffer<u8> leaves_d(leaves_a.getVolume());
	for(s32 i=0; i<leaves_a.getVolume(); i++)
		leaves_d[i] = 0;

	// Force leaves at near the end of the trunk
	{
		s16 d = 1;
		for(s16 z=-d; z<=d; z++)
		for(s16 y=-d; y<=d; y++)
		for(s16 x=-d; x<=d; x++)
		{
			leaves_d[leaves_a.index(v3s16(x,y,z))] = 1;
		}
	}

	// Add leaves randomly
	for(u32 iii=0; iii<7; iii++)
	{
		s16 d = 1;

		v3s16 p(
			pr.range(leaves_a.MinEdge.X, leaves_a.MaxEdge.X-d),
			pr.range(leaves_a.MinEdge.Y, leaves_a.MaxEdge.Y-d),
			pr.range(leaves_a.MinEdge.Z, leaves_a.MaxEdge.Z-d)
		);

		for(s16 z=0; z<=d; z++)
		for(s16 y=0; y<=d; y++)
		for(s16 x=0; x<=d; x++)
		{
			leaves_d[leaves_a.index(p+v3s16(x,y,z))] = 1;
		}
	}

	// Blit leaves to vmanip
	for(s16 z=leaves_a.MinEdge.Z; z<=leaves_a.MaxEdge.Z; z++)
	for(s16 y=leaves_a.MinEdge.Y; y<=leaves_a.MaxEdge.Y; y++)
	for(s16 x=leaves_a.MinEdge.X; x<=leaves_a.MaxEdge.X; x++)
	{
		v3s16 p(x,y,z);
		p += p1;
		if(vmanip.m_area.contains(p) == false)
			continue;
		u32 vi = vmanip.m_area.index(p);
		if(vmanip.m_data[vi].getContent() != CONTENT_AIR
				&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
			continue;
		u32 i = leaves_a.index(x,y,z);
		if(leaves_d[i] == 1) {
			bool is_apple = pr.range(0,99) < 10;
			if(is_apple_tree && is_apple) {
				vmanip.m_data[vi] = applenode;
			} else {
				vmanip.m_data[vi] = leavesnode;
			}
		}
	}
}

// L-System tree LUA spawner
void spawn_ltree(ServerEnvironment *env, v3s16 p0, INodeDefManager *ndef, TreeDef tree_definition)
{
	ServerMap *map = &env->getServerMap();
	std::map<v3s16, MapBlock*> modified_blocks;
	ManualMapVoxelManipulator vmanip(map);
	v3s16 tree_blockp = getNodeBlockPos(p0);
	vmanip.initialEmerge(tree_blockp - v3s16(1,1,1), tree_blockp + v3s16(1,3,1));
	make_ltree (vmanip, p0, ndef, tree_definition);
	vmanip.blitBackAll(&modified_blocks);

	// update lighting
	shared_map<v3s16, MapBlock*> lighting_modified_blocks;
	lighting_modified_blocks.insert(modified_blocks.begin(), modified_blocks.end());
	map->updateLighting(lighting_modified_blocks, modified_blocks, 100);
	// Send a MEET_OTHER event
	MapEditEvent event;
	event.type = MEET_OTHER;
	for(std::map<v3s16, MapBlock*>::iterator
		i = modified_blocks.begin();
		i != modified_blocks.end(); ++i)
	{
		event.modified_blocks.insert(i->first);
	}
	map->dispatchEvent(&event);
}

//L-System tree generator
void make_ltree(ManualMapVoxelManipulator &vmanip, v3s16 p0, INodeDefManager *ndef,
		TreeDef tree_definition)
{
	MapNode dirtnode(ndef->getId("mapgen_dirt"));

	PseudoRandom ps(tree_definition.seed+14002);
	// chance of inserting abcd rules
	double prop_a = 9;
	double prop_b = 8;
	double prop_c = 7;
	double prop_d = 6;

	//randomize tree growth level, minimum=2
	s16 iterations = tree_definition.iterations;
	if (tree_definition.iterations_random_level>0)
		iterations -= ps.range(0,tree_definition.iterations_random_level);
	if (iterations<2)
		iterations=2;

	s16 MAX_ANGLE_OFFSET = 5;
	double angle_in_radians = (double)tree_definition.angle*M_PI/180;
	double angleOffset_in_radians = (s16)(ps.range(0,1)%MAX_ANGLE_OFFSET)*M_PI/180;

	//initialize rotation matrix, position and stacks for branches
	core::matrix4 rotation;
	rotation = setRotationAxisRadians(rotation, M_PI/2,v3f(0,0,1));
	v3f position;
	position.X = p0.X;
	position.Y = p0.Y;
	position.Z = p0.Z;
	std::stack <core::matrix4> stack_orientation;
	std::stack <v3f> stack_position;

	//generate axiom
	std::string axiom = tree_definition.initial_axiom;
	for(s16 i=0; i<iterations; i++)
	{
		std::string temp = "";
		for(s16 j=0; j<(s16)axiom.size(); j++)
		{
			char axiom_char = axiom.at(j);
			switch (axiom_char)
			{
			case 'A':
				temp+=tree_definition.rules_a;
				break;
			case 'B':
				temp+=tree_definition.rules_b;
				break;
			case 'C':
				temp+=tree_definition.rules_c;
				break;
			case 'D':
				temp+=tree_definition.rules_d;
				break;
			case 'a':
				if (prop_a >= ps.range(1,10))
					temp+=tree_definition.rules_a;
				break;
			case 'b':
				if (prop_b >= ps.range(1,10))
					temp+=tree_definition.rules_b;
				break;
			case 'c':
				if (prop_c >= ps.range(1,10))
					temp+=tree_definition.rules_c;
				break;
			case 'd':
				if (prop_d >= ps.range(1,10))
					temp+=tree_definition.rules_d;
				break;
			default:
				temp+=axiom_char;
				break;
			}
		}
		axiom=temp;
	}

	//make sure tree is not floating in the air
	if (tree_definition.trunk_type == "double")
	{
		tree_node_placement(vmanip,v3f(position.X+1,position.Y-1,position.Z),dirtnode);
		tree_node_placement(vmanip,v3f(position.X,position.Y-1,position.Z+1),dirtnode);
		tree_node_placement(vmanip,v3f(position.X+1,position.Y-1,position.Z+1),dirtnode);
	}
	if (tree_definition.trunk_type == "crossed")
	{
		tree_node_placement(vmanip,v3f(position.X+1,position.Y-1,position.Z),dirtnode);
		tree_node_placement(vmanip,v3f(position.X-1,position.Y-1,position.Z),dirtnode);
		tree_node_placement(vmanip,v3f(position.X,position.Y-1,position.Z+1),dirtnode);
		tree_node_placement(vmanip,v3f(position.X,position.Y-1,position.Z-1),dirtnode);
	}

	/* build tree out of generated axiom

	Key for Special L-System Symbols used in Axioms

    G  - move forward one unit with the pen up
    F  - move forward one unit with the pen down drawing trunks and branches
    f  - move forward one unit with the pen down drawing leaves (100% chance)
    T  - move forward one unit with the pen down drawing trunks only
    R  - move forward one unit with the pen down placing fruit
    A  - replace with rules set A
    B  - replace with rules set B
    C  - replace with rules set C
    D  - replace with rules set D
    a  - replace with rules set A, chance 90%
    b  - replace with rules set B, chance 80%
    c  - replace with rules set C, chance 70%
    d  - replace with rules set D, chance 60%
    +  - yaw the turtle right by angle degrees
    -  - yaw the turtle left by angle degrees
    &  - pitch the turtle down by angle degrees
    ^  - pitch the turtle up by angle degrees
    /  - roll the turtle to the right by angle degrees
    *  - roll the turtle to the left by angle degrees
    [  - save in stack current state info
    ]  - recover from stack state info

    */

	s16 x,y,z;
	for(s16 i=0; i<(s16)axiom.size(); i++)
	{
		char axiom_char = axiom.at(i);
		core::matrix4 temp_rotation;
		temp_rotation.makeIdentity();
		v3f dir;
		switch (axiom_char)
		{
		case 'G':
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;
		case 'T':
			tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z),tree_definition);
			if (tree_definition.trunk_type == "double" && !tree_definition.thin_branches)
			{
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z+1),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z+1),tree_definition);
			}
			if (tree_definition.trunk_type == "crossed" && !tree_definition.thin_branches)
			{
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X-1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z+1),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z-1),tree_definition);
			}
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;
		case 'F':
			tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z),tree_definition);
			if ((stack_orientation.empty() && tree_definition.trunk_type == "double") ||
				(!stack_orientation.empty() && tree_definition.trunk_type == "double" && !tree_definition.thin_branches))
			{
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z+1),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z+1),tree_definition);
			}
			if ((stack_orientation.empty() && tree_definition.trunk_type == "crossed") ||
				(!stack_orientation.empty() && tree_definition.trunk_type == "crossed" && !tree_definition.thin_branches))
			{
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X-1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z+1),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z-1),tree_definition);
			}
			if (stack_orientation.empty() == false)
			{
				s16 size = 1;
				for(x=-size; x<=size; x++)
					for(y=-size; y<=size; y++)
						for(z=-size; z<=size; z++)
							if (abs(x) == size && abs(y) == size && abs(z) == size)
							{
								tree_leaves_placement(vmanip,v3f(position.X+x+1,position.Y+y,position.Z+z),ps.next(), tree_definition);
								tree_leaves_placement(vmanip,v3f(position.X+x-1,position.Y+y,position.Z+z),ps.next(), tree_definition);
								tree_leaves_placement(vmanip,v3f(position.X+x,position.Y+y,position.Z+z+1),ps.next(), tree_definition);
								tree_leaves_placement(vmanip,v3f(position.X+x,position.Y+y,position.Z+z-1),ps.next(), tree_definition);
							}
			}
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;
		case 'f':
			tree_single_leaves_placement(vmanip,v3f(position.X,position.Y,position.Z),ps.next() ,tree_definition);
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;
		case 'R':
			tree_fruit_placement(vmanip,v3f(position.X,position.Y,position.Z),tree_definition);
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;

		// turtle orientation commands
		case '[':
			stack_orientation.push(rotation);
			stack_position.push(position);
			break;
		case ']':
			rotation=stack_orientation.top();
			stack_orientation.pop();
			position=stack_position.top();
			stack_position.pop();
			break;
		case '+':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians+angleOffset_in_radians,v3f(0,0,1));
			rotation*=temp_rotation;
			break;
		case '-':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians+angleOffset_in_radians,v3f(0,0,-1));
			rotation*=temp_rotation;
			break;
		case '&':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians+angleOffset_in_radians,v3f(0,1,0));
			rotation*=temp_rotation;
			break;
		case '^':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians+angleOffset_in_radians,v3f(0,-1,0));
			rotation*=temp_rotation;
			break;
		case '*':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians,v3f(1,0,0));
			rotation*=temp_rotation;
			break;
		case '/':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians,v3f(-1,0,0));
			rotation*=temp_rotation;
			break;
		default:
			break;
		}
	}
}

void tree_node_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		MapNode node)
{
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = node;
}

void tree_trunk_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		TreeDef &tree_definition)
{
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = tree_definition.trunknode;
}

void tree_leaves_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		PseudoRandom ps ,TreeDef &tree_definition)
{
	MapNode leavesnode=tree_definition.leavesnode;
	if (ps.range(1,100) > 100-tree_definition.leaves2_chance)
		leavesnode=tree_definition.leaves2node;
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;	
	if (tree_definition.fruit_chance>0)
	{
		if (ps.range(1,100) > 100-tree_definition.fruit_chance)
			vmanip.m_data[vmanip.m_area.index(p1)] = tree_definition.fruitnode;
		else
			vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
	}
	else if (ps.range(1,100) > 20)
		vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
}

void tree_single_leaves_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		PseudoRandom ps, TreeDef &tree_definition)
{
	MapNode leavesnode=tree_definition.leavesnode;
	if (ps.range(1,100) > 100-tree_definition.leaves2_chance)
		leavesnode=tree_definition.leaves2node;
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
		&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
}

void tree_fruit_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		TreeDef &tree_definition)
{
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
		&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = tree_definition.fruitnode;
}

irr::core::matrix4 setRotationAxisRadians(irr::core::matrix4 M, double angle, v3f axis)
{
	double c = cos(angle);
	double s = sin(angle);
	double t = 1.0 - c;

	double tx  = t * axis.X;
	double ty  = t * axis.Y;
	double tz  = t * axis.Z;
	double sx  = s * axis.X;
	double sy  = s * axis.Y;
	double sz  = s * axis.Z;

	M[0] = tx * axis.X + c;
	M[1] = tx * axis.Y + sz;
	M[2] = tx * axis.Z - sy;

	M[4] = ty * axis.X - sz;
	M[5] = ty * axis.Y + c;
	M[6] = ty * axis.Z + sx;

	M[8]  = tz * axis.X + sy;
	M[9]  = tz * axis.Y - sx;
	M[10] = tz * axis.Z + c;
	return M;
}

v3f transposeMatrix(irr::core::matrix4 M, v3f v)
{
	v3f translated;
	double x = M[0] * v.X + M[4] * v.Y + M[8]  * v.Z +M[12];
	double y = M[1] * v.X + M[5] * v.Y + M[9]  * v.Z +M[13];
	double z = M[2] * v.X + M[6] * v.Y + M[10] * v.Z +M[14];
	translated.X=x;
	translated.Y=y;
	translated.Z=z;
	return translated;
}

void make_jungletree(VoxelManipulator &vmanip, v3s16 p0,
		INodeDefManager *ndef, int seed)
{
	/*
		NOTE: Tree-placing code is currently duplicated in the engine
		and in games that have saplings; both are deprecated but not
		replaced yet
	*/
	content_t c_tree   = ndef->getId("mapgen_jungletree");
	content_t c_leaves = ndef->getId("mapgen_jungleleaves");
	if (c_tree == CONTENT_IGNORE)
		c_tree = ndef->getId("mapgen_tree");
	if (c_leaves == CONTENT_IGNORE)
		c_leaves = ndef->getId("mapgen_leaves");

	MapNode treenode(c_tree);
	MapNode leavesnode(c_leaves);

	PseudoRandom pr(seed);
	for(s16 x=-1; x<=1; x++)
	for(s16 z=-1; z<=1; z++)
	{
		if(pr.range(0, 2) == 0)
			continue;
		v3s16 p1 = p0 + v3s16(x,0,z);
		v3s16 p2 = p0 + v3s16(x,-1,z);
		u32 vi1 = vmanip.m_area.index(p1);
		u32 vi2 = vmanip.m_area.index(p2);
		
		if (vmanip.m_area.contains(p2) &&
			vmanip.m_data[vi2].getContent() == CONTENT_AIR)
			vmanip.m_data[vi2] = treenode;
		else if (vmanip.m_area.contains(p1) &&
				vmanip.m_data[vi1].getContent() == CONTENT_AIR)
			vmanip.m_data[vi1] = treenode;
	}
	vmanip.m_data[vmanip.m_area.index(p0)] = treenode;

	s16 trunk_h = pr.range(8, 12);
	v3s16 p1 = p0;
	for (s16 ii=0; ii<trunk_h; ii++)
	{
		if (vmanip.m_area.contains(p1)) {
			u32 vi = vmanip.m_area.index(p1);
			if (vmanip.m_data[vi].getContent() == CONTENT_AIR)
				vmanip.m_data[vi] = treenode;
		}
		p1.Y++;
	}

	// p1 is now the last piece of the trunk
	p1.Y -= 1;

	s16 size = pr.range(2, 4);
	VoxelArea leaves_a(v3s16(-size,-pr.range(2, 4),-size), v3s16(size, pr.range(2, 4), size));
	//SharedPtr<u8> leaves_d(new u8[leaves_a.getVolume()]);
	Buffer<u8> leaves_d(leaves_a.getVolume());
	for(s32 i=0; i<leaves_a.getVolume(); i++)
		leaves_d[i] = 0;

	// Force leaves at near the end of the trunk
	{
		s16 d = 1;
		for(s16 z=-d; z<=d; z++)
		for(s16 y=-d; y<=d; y++)
		for(s16 x=-d; x<=d; x++)
		{
			leaves_d[leaves_a.index(v3s16(x,y,z))] = 1;
		}
	}

	// Add leaves randomly
	for(u32 iii=0; iii<30; iii++)
	{
		s16 d = 1;

		v3s16 p(
			pr.range(leaves_a.MinEdge.X, leaves_a.MaxEdge.X-d),
			pr.range(leaves_a.MinEdge.Y, leaves_a.MaxEdge.Y-d),
			pr.range(leaves_a.MinEdge.Z, leaves_a.MaxEdge.Z-d)
		);

		for(s16 z=0; z<=d; z++)
		for(s16 y=0; y<=d; y++)
		for(s16 x=0; x<=d; x++)
		{
			leaves_d[leaves_a.index(p+v3s16(x,y,z))] = 1;
		}
	}

	// Blit leaves to vmanip
	for(s16 z=leaves_a.MinEdge.Z; z<=leaves_a.MaxEdge.Z; z++)
	for(s16 y=leaves_a.MinEdge.Y; y<=leaves_a.MaxEdge.Y; y++)
	for(s16 x=leaves_a.MinEdge.X; x<=leaves_a.MaxEdge.X; x++)
	{
		v3s16 p(x,y,z);
		p += p1;
		if(vmanip.m_area.contains(p) == false)
			continue;
		u32 vi = vmanip.m_area.index(p);
		if (vmanip.m_data[vi].getContent() != CONTENT_AIR &&
			vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
			continue;
		u32 i = leaves_a.index(x,y,z);
		if(leaves_d[i] == 1)
			vmanip.m_data[vi] = leavesnode;
	}
}

void make_cavetree(ManualMapVoxelManipulator &vmanip, v3s16 p0,
		bool is_jungle_tree, INodeDefManager *ndef, int seed)
{
	MapNode treenode(ndef->getId(is_jungle_tree ? "mapgen_jungletree" : "mapgen_tree"));
	MapNode leavesnode(ndef->getId(is_jungle_tree ? "mapgen_jungleleaves" : "mapgen_leaves"));

	PseudoRandom pr(seed);
	s16 trunk_h = pr.range(2, pr.range(2, 5));
	v3s16 p1 = p0;
	for(s16 ii=0; ii<trunk_h; ii++)
	{
		if(vmanip.m_area.contains(p1)) {
			if(vmanip.getNodeNoExNoEmerge(p1).getContent() != CONTENT_AIR)
				return;
			if (ii == 0 && vmanip.getNodeNoExNoEmerge(p1).getLight(LIGHTBANK_DAY, ndef) == LIGHT_SUN)
				return;
			vmanip.m_data[vmanip.m_area.index(p1)] = treenode;
		}
		p1.Y++;
	}
	if(vmanip.m_area.contains(p1))
		if(vmanip.getNodeNoExNoEmerge(p1).getContent() != CONTENT_AIR)
			return;
		vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
}

}; // namespace treegen
