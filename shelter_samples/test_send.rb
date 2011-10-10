shelter :A do
  class Object
    alias hoge send
    def a
      p current_node
      1
    end
  end
end
shelter :B do
  import :A
end
shelter_eval :A do
  p self.a
  p self.send(:a)
  p self.hoge(:a)
end
shelter_eval :B do
  p self.a
  p self.send(:a)
  p self.hoge(:a)
end
