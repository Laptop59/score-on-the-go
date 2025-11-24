# Score on the Go: Edit Compression

This article will describe how to compress a ballfile (for edits in SOTG) to a string of digits.
Balls, holds (only head), pits ("), mines, and bouncy balls, and PW, PS and DUAL changes are all under the same encoding:
each of them for the purposes of this article be called a **macrocode**.

Example:
```
measureStart ball mine PW change         The '___'s represent the macrocodes.
............ ---- ---- ---------
```

Before detailing into the things that get encoded, let's take a look at their components:

Remember to check for provided assertions (marked with Ɐ)

## Components: The Building Blocks

|Component|Detail|How to encode|
|---------|------|-------------|
|**Δub**|1-3 digits|This is the number of 'unit beats' (represented by *ub*) since the last macrocode. It is encoded in the following manner:|
||0|+0 ub|
||1|+1 ub|
||2|+2 ub|
||3|+3 ub|
||4|+4 ub|
||5|+6 ub|
||6|+8 ub|
||||
||70|+5 ub|
||71|+7 ub|
||72|+9 ub|
||73|+10 ub|
||74|+11 ub|
||75|+12 ub|
||76|+13 ub|
||77|+14 ub|
||78|+15 ub|
||||
||79|+16 ub|
||8*XY*|+*XY* ub|
||9*XY*|+1*XY* ub|
|||**Ɐ `uB < 200`**|
|**x**|3 digits|This represents the x-coordinate of a ball, mine, hold head or pit head. which stays constant. It is encoded according to the formula:|
|||`encoded = round(x*2)+500`|
|||For this formula to work properly, `x` must be representable as an integer or halves and be in the range `[-250, 249.5)`|
|||It can then easily be decoded with `decoded = (encoded-500)/2`.|
|||**Ɐ `249.75 > uB > -250.25`**|
|**>>>**|2-5 digits|This represents the speed of a ball. Ways of encoding them are given below:|
||*(A=0~3)B*|Represents a speed of `(10A+B+1)/10`|
||e.g.||
||00|**x0.1**|
||01|**x0.2**|
||09|**x1.0**|
||11|**x1.2**|
||39|**x4.0**|
||*(A=4~8)BC*|Represents a speed of `(100(A-4)+10B+C+1)/100`|
||e.g.||
||400|**x0.01** = `(100(4-4)+10(0)+(0)+1)/100`|
||401|**x0.02** = `(100(4-4)+10(0)+(1)+1)/100`|
||409|**x0.10** = `(100(4-4)+10(0)+(9)+1)/100`|
||430|**x0.31** = `(100(4-4)+10(3)+(0)+1)/100`|
||431|**x0.32** = `(100(4-4)+10(3)+(1)+1)/100`|
||439|**x0.40** = `(100(4-4)+10(3)+(9)+1)/100`|
||830|**x4.31** = `(100(8-4)+10(3)+(0)+1)/100`|
||831|**x4.32** = `(100(8-4)+10(3)+(1)+1)/100`|
||899|**x5.00** = `(100(8-4)+10(9)+(9)+1)/100`|
||*9ABCD*|Represents a speed of `(1000A+100B+10C+D+1)/1000`|
||e.g.||
||90000|**x0.001**|
||91523|**x1.524**|
||97779|**x7.780**|
||99999|**x10.000**|
|||***IMPORTANT***: Values outside encodable range must be rounded.|
|||**Ɐ `0.001 < speed < 10` (or can be reasonably approximated)**|
|**/\\/**|Varies|Represents a list of nodes in a hold body or pit body.|
|||Each tail node is stored in the form **Δb** **x** where Δb denotes the change in beats since the last tail node (or head of hold/pit)|
|||**Δb** is stored in the following way:|
|||Let **Δmb** = `round(48*Δb)` (minibeats)|
|||Then **Δb** is stored as `(len(Δmb)-1)` `Δmb`|
|||Condition: `Δmb < 1_000_000_000`.|
||e.g.||
||148|`48mb` = `1.000` beats|
||01|`1mb` = `0.021` beats|
||111|`11mb` = `0.229` beats|
||31234|`1234mb` = `25.708` beats|
||5123456|`123456mb` = `2572.000` beats|
|||**Ɐ `0 <= Δmb < 1e9`**|
|||Nodes are stored in the following way: (no, the tail node at Δb=0 is not stored)|
|||`Δb1` `x1` `Δb2` `x2` `Δb3` `x3` ... `ΔbN` `xN` 9|
|||Here, `9` indicates ending of `N` nodes.|
|**vvv**|(3-5 digits) `RR` `ub`|Represents the bouncy nature of a ball where RR represents (number of respawns - 1)|
|||**Ɐ `00 <= RR <= 98` or `1 <= r <= 99`** (`r` is number of respawns)|
|**Pw**|(3 digits) `XXX`|Specifies the new width of the paddle at this change where XXX is calculated by the formula:|
|||`XXX = pad(width, 3)`|
|||**Ɐ `0 <= width <= 454`**|
|**Ps**|(3 digits) `XXX`|Specifies the new speed of the paddle (pixels per second at slow speed) at this change where XXX is calculated by the formula:|
|||`XXX = pad(speed * 2, 3)`|
|||**Ɐ `0 <= speed <= 499.75`**|

## Macrocodes

|Specification|Extra|Details|
|-------------|-----|-------|
|0`X`||Specifies a new measure (this resets initial uB to be equal to `0` and sets a new 'conversion rate')|
|||The 0 **NEEDS TO BE SPECIFIED** for the first measure **ONLY IF** if it has macrocodes `measure 0 - beats [0, 4)`|
|||The values of `X` sets the conversion rate as follows:|
|||Note: `1b = 1 beat`, `48 mb = 1 b`!|
||`X` = 0|`1 ub = 96 mb`&emsp;2<sup>nd</sup>|
||`X` = 1|`1 ub = 48 mb`&emsp;4<sup>th</sup>|
||`X` = 2|`1 ub = 24 mb`&emsp;8<sup>th</sup>|
||`X` = 3|`1 ub = 16 mb`&emsp;12<sup>th</sup>|
||`X` = 4|`1 ub = 12 mb`&emsp;16<sup>th</sup>|
||`X` = 5|`1 ub = 8 mb `&emsp;24<sup>th</sup>|
||`X` = 6|`1 ub = 6 mb `&emsp;32<sup>nd</sup>|
||`X` = 7|`1 ub = 4 mb `&emsp;48<sup>th</sup>|
||`X` = 8|`1 ub = 3 mb `&emsp;64<sup>nd</sup>|
||`X` = 9|`1 ub = 1 mb `&emsp;192<sup>nd</sup>|
|1 `Δub` `x` `>>>`||Specifies a normal ball with the specified properties.|
|2 `Δub` `x` `>>>`||Specifies a mine ball with the specified properties.|
|3 `Δub` `x` `>>>` `/\/`||Specifies a hold ball with the specified properties.|
|4 `Δub` `x` `>>>` `/\/`||Specifies a pit ball with the specified properties.|
|5 `Δub` `x` `>>>` `vvv`||Specifies a bouncy ball with the specified properties.|
|6 `Δub` `Pw`||Specifies a paddle width change with the specified properties.|
|7 `Δub` `Ps`||Specifies a paddle speed change with the specified properties.|
|8 `Δub`||Specifies a dual change with the specified properties by inverting the current flag.|
|9 `L` `XXX...`||Skips a number of measures which will not have any macrocodes.|
|||`L` is the number of digits of measures (given by `XXX...`) to skip subtracted by 1.|
|||Useful to skip an integral number of measures.|
||e.g.|Skipping 123 measures can be done with `92123`.|
|||**Note:**|
|||`0?` can be used to skip ONE measure.|