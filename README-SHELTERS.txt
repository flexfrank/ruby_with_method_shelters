"Method Shelters" is a module system which resolves conflicts caused by Open Class.

We had a presentation at Rubykaigi 2011.
The slides of the presentation is available on 
http://www.slideshare.net/flexfrank/method-shelters-another-way-to-resolve-class-extension-conflicts

* Example
To define a method shelter, use "shelter" method and define classes and methods
in the block of the "shelter" method.

  shelter :ShelterA do
    class Array
      def sum
        # do  something
      end
    end
  end

To use methods defined in another shelter, use "import" method.

  shelter :ShelterB do
    import :ShelterA
  end

To enable a shelter in top level, use "shelter_eval" method.

  shelter_eval :ShelterB do
    [1,2,3].sum #=> 6
  end
