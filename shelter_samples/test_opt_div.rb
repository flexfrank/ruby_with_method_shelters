if self.respond_to?(:shelter)
  shelter :A do
    class Fixnum
      def /(o)
        Rational(self,o)
      end
    end
  end
else
  class Fixnum
    def /(o)
      Rational(self,o)
    end
  end
end
TIMES=50000000
a=1.0
if self.respond_to?(:shelter_eval)
  shelter_eval :A do
    p 1/10
  end
  TIMES.times do
     a / 1.0
  end
  p a
else
  p 1/10
  TIMES.times do
     a / 1.0
  end
  p a
end
