local m = {}

function m.setup(e)
	log_info("2D example running!")

	local sprite = spawn()
	Transform2D.set(sprite, Vec2.new(96, 64), Vec2.new(1, 1), Vec2.new(), 0)
	Sprite.set(sprite, "/atlas.json", "frame0")

	local animated = spawn()
	Transform2D.set(animated, Vec2.new(224, 64), Vec2.new(1, 1), Vec2.new(), 0)
	AnimatedSprite.set(animated, "/atlas.json", "walk")
end

return m
