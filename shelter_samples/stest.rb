class Foo
end
shelter :S8 do
  class Object
    def uncallable
      1
    end
  end
end
shelter :S7 do
  class Object
    def hoge
      :s7_hoge
    end
  end
  hide
    import :S8
end

shelter :S6 do
  class Object
    def bar
      p 1
      :s6_bar
    end
  end
end
shelter :S5 do
end
shelter :S4 do
  import :S6
end
shelter :S3 do
  import :S7
end
shelter :S2 do
  import :S5
  import :S7
  class Object
    def foo
      :s2_foo
    end
  end
end
shelter :Rat do
  class Fixnum
    def div(o)
      Rational(self,o)
    end
  end
end
shelter :F do
  class Foo
    def xxx
      [:sub, super]
    end
  end
end
shelter :S1 do
  import :S2
  import :S3
  import :F
  class Object
    def aaa
      :s1_aaa
    end
  end
 hide
  import :Rat
  import :S4
end

class Object
  def xxx
    [:xxx,self.foo]
  end
end
class Foo
  def aaa
    [:super,super()]
  end
  def test
    p self.aaa
    p self.bar
    p self.hoge
    p self.foo
    p 1.div 2
    p self.xxx
    begin
      p self.uncallable
    rescue NoMethodError
    else
      raise
    end
  end
end

shelter_eval(:S1) do
  Foo.new.test
end
