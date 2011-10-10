if self.respond_to?(:shelter)
  shelter :A do
    class Float
      def +(o)
        nil
      end
    end
  end
else
  class Float
    unless ARGV[0]
      def +(o)
        nil
      end
    end
  end
end
TIMES=100000000
a=1
if self.respond_to?(:shelter_eval)
  shelter_eval :A do
    p 1.0+1.0
  end
  TIMES.times do
     a + 1
  end
  p a
else
  p 1.0 + 1.0
  TIMES.times do
     a + 1
  end
  p a
end
