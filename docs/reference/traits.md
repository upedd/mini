### 🚧 Work in progress document to document how traits should behave

# Traits

Trait may contain required members (methods or fields) and method definitions.

Reference paper: https://www.cs.cmu.edu/~aldrich/courses/819/Scha03aTraits.pdf
```
trait A {
    get_hello(); # required member
    say_hello() { # method definiton
        print(get_hello());
    }
}
```

## Using traits

Traits _copy and paste_ defined methods inside class body and then check if all requirements are fulfilled.

When including traits defined method names can be aliased or excluded.
In that case aliased or excluded method goes is _moved_ to requirements section of the trait.
Methods inside of class must be uniquely named to resolve conflicts one should use aliasing or excluding.
When including traits superclass overrides must be explicitly stated.
Traits must have flattening property (TODO: explain!).
```
class Foo: Bar {
    # aliasing and excluding 
    using A { get_hello as a_get_hello, exclude say_hello };
    
    # no implicit trait overrides
    override get_hello() {
        return a_get_hello();
    }
}
```

Traits can be composed of other traits. Composition rules are identical as with classes.
```
trait B {
    using A { say_hello as a_say_hello };
    fun say_hello() {
        print("hello from b");
    }
}
```


