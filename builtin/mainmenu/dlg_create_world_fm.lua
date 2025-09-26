
mg_preset = {
    math = 	{
		xenodreambuie1 = {
		    mg_math = '{"generator":"xenodreambuie", "N":12, "scale":0.003, "power":2, "julia_mode":1, "julia_a":-1.13}',
			mg_float_islands = 0,
		},
		aexion = {
		    mg_math           = '{"generator" : "aexion", "N" : 11,"cadd" : -1.0002,"size" : 30000 }',
		    water_level       = -30000,
			mg_float_islands = 0,
			static_spawnpoint = '0, 2, 0',
		},
        hypercomplex = {
			mg_math           = '{"generator":"hypercomplex", "scale":0.0001, "N":12, "interior_mode": 1, "center":{"x":20000, "y":1, "z":0}}',
			mg_float_islands = 0,
			static_spawnpoint = '0, 2, 0',
		},
        xenodreambuie2 = {
			mg_math           = '{"generator":"xenodreambuie", "N":20, "scale":0.0000001, "power":2, "julia_mode":1, "julia_a": 0.999, "julia_b": 0.1, "julia_c": 1.01, "center":{"x":6021817,"z":-11938439,"y":-3601800}}',
			mg_float_islands = 0,
		},
        rooms = {
			mg_math           = '{"generator":"rooms"}',
			light_ambient     = '1',
			mg_float_islands = 0,
		}
	},
    earth = {
        hong_kong  = { mg_earth = '{"center": {"z": 22.28422,   "x": 114.15996,    "y":0} }', },
        new_york   = { mg_earth = '{"center": {"z": 40.781137,  "x": -73.966487,   "y":0} }', },
        everest    = { mg_earth = '{"center": {"z": 27.9878279, "x": 86.923833,    "y":0} }', },
        big_canyon = { mg_earth = '{"center": {"z": 34.2139435, "x": -111.1582085, "y":0} }', },
        dolomites  = { mg_earth = '{"center": {"z": 46.4084595, "x": 11.8393018    } }', },
        matterhorn = { mg_earth = '{"center": {"z": 45.976400,  "x": 7.658600      } }', },
        dubai      = { mg_earth = '{"center": {"z": 25.19715,   "x": 55.27288      } }', },
        tokyo      = { mg_earth = '{"center": {"z": 35.6835103, "x": 139.7538318   } }', },
	}
}
