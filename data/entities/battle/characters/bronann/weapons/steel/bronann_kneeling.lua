-- Animation file descriptor
-- This file will describe the frames used to load an animation

animation = {

	-- The file to load the frames from
	image_filename = "data/entities/battle/characters/bronann/weapons/steel/bronann_kneeling.png",
	-- The number of rows and columns of images, will be used to compute
	-- the images width and height, and also the frames number (row x col)
	rows = 1,
	columns = 1,
	-- set the image dimensions on battles (in pixels)
	frame_width = 70.0,
	frame_height = 112.0,
	-- The frames duration in milliseconds
    frames = {
        [0] = { id = 0, duration = 1000, y_offset = 9.0 }
    }
}
