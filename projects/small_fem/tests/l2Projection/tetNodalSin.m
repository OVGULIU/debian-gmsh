close all;
clear all;

%% l2 [Mesh][Order]

%% Sin(10x) + Sin(10y) + Sin(10z)


l2 = zeros(5, 5);

%% log_0_*
l2(0 + 1, 1) = +1.209063e+00; 
l2(0 + 1, 2) = +1.093649e+00; 
l2(0 + 1, 3) = +7.258179e-01; 
l2(0 + 1, 4) = +3.311431e-01; 

l2(0 + 1, 5) = +3.061368e-01; 
%l2(0 + 1, 6) = +2.657874e-01; 

log_0_7 = +5.559542e-02; 
log_0_8 = +4.772116e-02; 

%% log_1_*
l2(1 + 1, 1) = +6.934718e-01; 
l2(1 + 1, 2) = +2.574699e-01; 
l2(1 + 1, 3) = +9.919870e-02; 
l2(1 + 1, 4) = +2.066949e-02; 

l2(1 + 1, 5) = +5.372951e-03; 
%l2(1 + 1, 6) = +1.346614e-03; 

%% log_2_*
l2(2 + 1, 1) = +2.512674e-01; 
l2(2 + 1, 2) = +5.033066e-02; 
l2(2 + 1, 3) = +7.196701e-03; 
l2(2 + 1, 4) = +9.757486e-04; 

l2(2 + 1, 5) = +8.712885e-05; 
%l2(2 + 1, 6) = +8.946592e-06; 

%% log_3_*
l2(3 + 1, 1) = +6.334412e-02; 
l2(3 + 1, 2) = +8.256671e-03; 
l2(3 + 1, 3) = +4.566923e-04; 
l2(3 + 1, 4) = +3.575723e-05; 

l2(3 + 1, 5) = +1.328543e-06; 
%l2(3 + 1, 6) = +7.699689e-08;

%% log_4_*
l2(4 + 1, 1) = +1.391785e-02; 
l2(4 + 1, 2) = +1.250348e-03; 
l2(4 + 1, 3) = +2.527752e-05; 
l2(4 + 1, 4) = +1.225044e-06; 

l2(4 + 1, 5) = +2.048546e-08;


h = [1, 1/2, 1/4, 1/8, 1/16];
p = [1:5];

P = size(p, 2);
H = size(h, 2);

delta = zeros(H - 1, P);

for i = 1:H-1
    delta(i, :) = ...
        (log10(l2(i + 1, :)) - log10(l2(i, :))) / ...
        (log10(1/h(i + 1))   - log10(1/h(i)));
end

delta'

figure;
loglog(1./h, l2, '-*');
grid;
