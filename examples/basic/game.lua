function Setup(e)
	log_info("Log from Lua!")
	Timer.set(e, 2, TimerMode.repeating)
	-- local t = Timer.get(e)
end

function Update(e)
	if Timer.get(e):tick().is_finished then
		log_info("Game timer tick!")
	end
end

function Destroy(e)
	log_info("Destroing game!")
end
