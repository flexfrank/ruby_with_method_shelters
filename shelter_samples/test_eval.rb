shelter :A do
  class Object
    def a
      :adesu
    end
  end
end
shelter_eval :A do
  self.instance_eval{p self.a}
  Object.class_eval{p self;p self.a}
end

