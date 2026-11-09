local m = {}

function m.setup(e)
	log_info("Log from Lua!")
	Timer.set(e, 2, TimerMode.repeating)
end

function m.update(e)
	if Timer.get(e):tick().is_finished then
		log_info("Game timer tick!")
	end
end

function m.destroy(e)
	log_info("Destroing game!")
end

return m
