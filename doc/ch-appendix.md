# Appendix

## One-dimensional Cubic Interpolation

With four sample values $V_0$, $V_1$, $V_2$ and $V_3$, cubic interpolation approximates
the curve segment connecting $V_1$ and $V_2$, by using the beginning and ending
slope, the curvature and the rate of curvature change to construct a cubic
polynomial.


The cubic polynomial starts out as:

* a)	$f(x) = w_3 x^3 + w_2 x^2 + w_1 x + w_0$

Where $0 <= x <= 1$, specifying the sample value of the curve segment between
$V_1$ and $V_2$ to obtain.

To calculate the coefficients $w_0…w_3$, we set out the following conditions:

* b)	$f(0)  = V_1$
* c)	$f(1)  = V_2$
* d)	$f'(0) = V_1'$
* e)	$f'(1) = V_2'$

We obtain $V_1'$ and $V_2'$ from the respecting slope triangles:

* f)	$V_1' = \frac {V_2 - V_0} {2}$
* g)	$V_2' = \frac {V_3 - V_1} {2}$

With (f)→(d) and (g)→(e) we get:

* h)	$f'(0) = \frac {V_2 - V_0} {2}$
* i)	$f'(1) = \frac {V_3 - V_1} {2}$

The derivation of $f(x)$ is:

* j)	$f'(x)  = 3 w_3 x^2 + 2 w_2 x + w_1$

From $x=0$ →(a), i.e. (b), we obtain $w_0$ and from $x=0$ →(j),
i.e. (h), we obtain $w_1$. With $w_0$ and $w_1$ we can solve the
linear equation system formed by (c)→(a) and (e)→(j)
to obtain $w_2$ and $w_3$.

$(c)→(a):	 	w_3 +   w_2 + \frac {V_2 - V_0} {2} + V_1 = V_2$

$(e)→(j):	 	3 w_3 + 2 w_2 + \frac {V_2 - V_0} {2}    = \frac {V_3 - V_1} {2}$

With the resulting coefficients:

$$
\begin{aligned}
    w_0 &= V_1 &                                        &(initial\:value)           \\
    w_1 &= \frac{V_2 - V_0} {2} &                       &(initial\:slope)           \\
    w_2 &= \frac{-V_3 + 4 V_2 - 5 V_1 + 2 V_0} {2} &    &(initial\:curvature)       \\
    w_3 &= \frac{V_3 - 3 V_2 + 3 V_1 - V_0} {2} &       &(rate\:change\:of\:curvature)
\end{aligned}
$$

Reformulating (a) to involve just multiplications and additions (eliminating power), we get:

* k)	$f(x) = ((w_3 x + w_2) x + w_1) x + w_0$

Based on $V_0…V_3$, $w_0…w_3$ and (k), we can now approximate all values of the
curve segment between $V_1$ and $V_2$.

However, for practical resampling applications where only a specific
precision is required, the number of points we need out of the curve
segment can be reduced to a finite amount.
Lets assume we require $n$ equally spread values of the curve segment,
then we can precalculate $n$ sets of $W_{0…3}[i]$, $i=[0…n]$, coefficients
to speed up the resampling calculation, trading memory for
computational performance. With $w_{0…3}$ in (a):

$$
\begin{alignedat} {2}
	f(x) \  &= &   \frac{V_3 - 3 V_2 + 3 V_1 - V_0} 2 x^3 \  +	& \\
		    &  & \frac{-V_3 + 4 V_2 - 5 V_1 + 2 V_0} 2 x^2 \ +	& \\
		    &  &                     \frac{V_2 - V_0} 2 x \  +	& \\
		    &  &                                   V1 \ \ \ \ 	&
\end{alignedat}
$$

sorted for $V_0…V_4$, we have:

* l) $\begin{aligned}
 f(x) \  = \  & V_3 \  (0.5 x^3 - 0.5 x^2) \  +		& \\
	          & V_2 \  (-1.5 x^3 + 2 x^2 + 0.5 x) \  +	& \\
			  & V_1 \  (1.5 x^3 - 2.5 x^2 + 1) \  +	& \\
			  & V_0 \  (-0.5 x^3 + x^2 - 0.5 x)		&
\end{aligned}$

With (l) we can solve $f(x)$ for all $x = \frac i n$, where $i = [0, 1, 2, …, n]$ by
substituting $g(i) = f(\frac i n)$ with

* m)	$g(i) = V_3 W_3[i] + V_2 W_2[i] + V_1 W_1[i] + V_0 W_0[i]$

and using $n$ precalculated coefficients $W_{0…3}$ according to:

$$
\begin{alignedat}{4}
        m      &= \frac i n                                \\
        W_3[i] &=&  0.5 m^3 & - & 0.5 m^2 &         &      \\
        W_2[i] &=& -1.5 m^3 & + &   2 m^2 & + 0.5 m &      \\
        W_1[i] &=&  1.5 m^3 & - & 2.5 m^2 &         & + 1  \\
        W_0[i] &=& -0.5 m^3 & + &     m^2 & - 0.5 m &
\end{alignedat}
$$

We now need to setup $W_{0…3}[0…n]$ only once, and are then able to
obtain up to $n$ approximation values of the curve segment between
$V_1$ and $V_2$ with four multiplications and three additions using (m),
given $V_0…V_3$.


## Modifier Keys

There seems to be a lot of inconsistency in the behaviour of modifiers
(shift and/or control) with regards to GUI operations like selections
and drag and drop behaviour.

According to the Gtk+ implementation, modifiers relate to DND operations
according to the following list:

Table: GDK drag-and-drop modifier keys

Modifier            Operation       Note / X-Cursor
--------------- --- --------------- ---------------------
none             →  copy            (else move (else link))
`SHIFT`          →  move            `GDK_FLEUR`
`CTRL`           →  copy            `GDK_PLUS`, `GDK_CROSS`
`SHIFT+CTRL`     →  link            `GDK_UL_ANGLE`

Regarding selections, the following email provides a short summary:

> From: Tim Janik <timj@gtk.org>
> To: Hacking Gnomes <Gnome-Hackers@gnome.org>
> Subject: modifiers for the second selection
> Message-ID: <Pine.LNX.4.21.0207111747190.12292-100000@rabbit.birnet.private>
> Date: Thu, 11 Jul 2002 18:10:52 +0200 (CEST)
>
> hi all,
>
> in the course of reimplementing drag-selection for a widget,
> i did a small survey of modifier behaviour in other (gnome/
> gtk) programs and had to figure that there's no current
> standard behaviour to adhere to:
>
> for all applications, the first selection works as
> expected, i.e. press-drag-release selects the region
> (box) the mouse was draged over. also, starting a new
> selection without pressing any modifiers simply replaces
> the first one. differences occour when holding a modifier
> (shift or ctrl) when starting the second selection.
>
> Gimp:
> Shift upon button press:        the new seleciton is added to the existing one
> Ctrl upon button press:         the new selection is subtracted from the
>                                 existing one
> Shift during drag:              the selection area (box or circle) has fixed
>                                 aspect ratio
> Ctrl during drag:               the position of the initial button press
>                                 serves as center of the selected box/circle,
>                                 rather than the upper left corner
>
> Gnumeric:
> Shift upon button press:        the first selection is resized
> Ctrl upon button press:         the new seleciton is added to the existing one
>
> Abiword (selecting text regions):
> Shift upon button press:        the first selection is resized
> Ctrl upon button press:         triggers a compound (word) selection that
>                                 replaces the first selection
>
> Mozilla (selecting text regions):
> Shift upon button press:        the first selection is resized
>
> Nautilus:
> Shift or Ctrl upon buttn press: the new selection is added to or subtracted
>                                 from the first selection, depending on whether
>                                 the newly selected region was selected before.
>                                 i.e. implementing XOR integration of the newly
>                                 selected area into the first.
>
> i'm not pointing this out to start a flame war over what selection style
> is good or bad and i do realize that different applications have
> different needs (i.e. abiword does need compound selection, and
> the aspect-ratio/centering style for gimp wouldn't make too much
> sense for text), but i think for the benfit of the (new) users,
> there should me more consistency regarding modifier association
> with adding/subtracting/resizing/xoring to/from existing selections.
>
> ---
> ciaoTJ
