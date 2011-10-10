if self.respond_to?(:shelter)
  shelter :A do
    class Object
      def method_missing(name,*args)
        [name,args]
      end
    end
  end
else
  class Object
    def method_missing(name,*args)
      [name,args]
    end
  end
end
TIMES=200000
if(self.respond_to?(:shelter_eval))
  shelter_eval :A do
    TIMES.times do
      self.hoge
    end
  end
else
  TIMES.times do
    self.hoge
  end
end
