close all;
clear all;

%% l2 [Order][Mesh]

%% f = Sin(10x)

h = [1, 1/2, 1/4, 1/8, 1/16];
p = [1:6];

l2 = ...
    [...
        +1.131157e+00 , +6.525446e-01 , +2.322987e-01 , +5.779563e-02 , +1.457881e-02 ; ...
        +1.075659e+00 , +3.259000e-01 , +4.009796e-02 , +5.697928e-03 , +7.230599e-04 ; ...
        +7.064195e-01 , +8.558323e-02 , +7.916540e-03 , +4.836531e-04 , +3.047550e-05 ; ...
        +5.071560e-01 , +2.786054e-02 , +7.985148e-04 , +2.830611e-05 , +8.941516e-07 ; ...
        +1.371007e-01 , +4.608069e-03 , +1.056545e-04 , +1.596523e-06 , +2.514349e-08 ; ...
        +9.687180e-02 , +1.094897e-03 , +7.536993e-06 , +6.645726e-08 , +5.227335e-10 ; ...
    ];

P = size(p, 2);
H = size(h, 2);

delta = zeros(P, H - 1);

for i = 1:H-1
    delta(:, i) = ...
        (log10(l2(:, i + 1)) - log10(l2(:, i))) / ...
        (log10(1/h(i + 1))   - log10(1/h(i)));
end

delta

figure;
loglog(1./h, l2', '-*');
grid;
