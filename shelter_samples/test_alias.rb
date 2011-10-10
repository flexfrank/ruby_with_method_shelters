shelter :A do
  class Object
    alias foo to_s
    def x
      1
    end
    alias y x
    alias_method :z, :x
  end
end
shelter_eval :A do
  p self.foo
  p self.y
  p self.z
end
p self.foo
