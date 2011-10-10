def x
  p :x
  y
  b
end
shelter :A do
  class Object
    def y
      p :y
    end
  end
  hide
  class Object
    def a
      x
      p :a
    end
    def b
      p :b
    end
  end
end
shelter_eval :A do
  a
end
