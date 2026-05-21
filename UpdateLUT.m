function [ LUTxx, LUTyy, status] = UpdateLUT( len, xx, yy)
status = 0;
LUTxx = [];
LUTyy = [];

% проверим монотонность xx, yy
if any( diff(xx) <= 0)
    status = 1;
end

if any( diff(yy) >= 0)
    status = 1;
end

if status == 0
    q = len - length(xx); % количество недостоющих точек
    if q > 0
        xx_pad = ( -q : 1 : -1) * 0.1 + xx(1); % коды
        yy_pad = ones( 1, q) * yy(1); % толщины
        LUTxx = [ xx_pad xx];
        LUTyy = [ yy_pad yy];
    end
end

end