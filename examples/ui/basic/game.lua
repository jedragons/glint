function Setup(e)
	e.counter = 0
	Stopwatch.set(e)
end

function Destroy(e)
	_ = e
end

function Update(e)
	local sw = Stopwatch.get(e)
	sw:tick()

	local flags = UiFlags.title | UiFlags.closable | UiFlags.movable
	ui():window("Lua UI", Rect.new(100, 100, 400, 220), flags, function()
		ui():layout_row_static(70, 200, 1)
		if ui():button_label("Button") then
			e.counter = e.counter + 1
			log_info("Button clicked", e.counter, "times! Since last press", sw.elapsed, "seconds elapsed.")
			sw:reset()
		end
	end)
end
