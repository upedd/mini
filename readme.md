### 🚧 Under Construction!!! 🚧
Note that Bite is in heavy development and most of the features have not been yet implemented.
# Bite 
Bite (formerly mini) is a multi-paradigm dynamically typed scripting language
made to be fun and easy to use and embed into other applications.

Made to be used in bite-100 fantasy console (soon™) but is versatile enough that
can be used in other applications as well.

## Photos
![image](https://github.com/user-attachments/assets/714ffb79-d40f-44c5-bac9-0f17cab3efae)
![image](https://github.com/user-attachments/assets/71c3830c-ab7f-4a6d-a9d7-959e6f76bb0b)


## Features 🤯

- Easy to embed into c++ applications
- Writen using very modern c++ 
- Modern and clean syntax
- Fast enough for a scripting language
- Well documented and structured implementation

## Taste of Bite 🍕
```
# lines starting with # are comments

# variables are defined using "let" keyword
let awesomeness = 10; # integers
let bugs = 4.5; # floating point numbers
let review = "best language ever"; # strings
let is_bite_best_language_ever = true; # booleans
let critics = nil; # empty value

# you assign values to variables like u would expect 
awesomness = 11;

# you may change change type of variable using assigment
is_bite_best_language_ever = "of course!!!";

# you can use bite as your calculator (all math operations are here)
let amazingness = awesomness * (bugs + 10); 

# even bitwise ones
let amazingness_times_two = amazingess << 1;

# if statements are also here
if (bugs < 10) {
    bugs = 10;
} else if (bugs > 100) {
    amazingness = 10000000;
} else {
    critics = "me";
}

# while loops also checking in!
while (amazingness < 100) {
    amazingness = amazingness * 2;
}

# and of course functions also came!
fun add(a, b) {
    return a + b;
}

review = add(amazingness, awesomness);
```

## Installation 🔨
### Requirements
- very recent version of c++ compiler (gcc >= 13)
- cmake >= 3.22
### instructions
```shell
git clone https://github.com/upedd/mini.git
cd mini
cmake -S . -B build # Your bite executable is now in build directory
```
## Usage 
```shell
./bite [path to your bite file]
```

## Acknowledgments
- Robert Nystrom and his [Crafting Interpreters](https://craftinginterpreters.com/)
