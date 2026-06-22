classdef FromApp
    % FROMAPP Класс конфигурации данных, передаваемых из интерфейса в Simulink
    
    properties
        % Уставки (скаляры)
        Amplitude (1,1) double = 800.0
        Periods   (1,1) double = 2.0
        Sharpness (1,1) double = 2.0
        
        % Таблицы калибровки LUT (векторы по 25 элементов)
        xx        (1,25) double = [41343 41343.1 41343.2 41343.3 41343.4 41343.5 41343.6 41343.7 41343.8 41343.9 41344 41682 41973 42429 43533 44717 46388 47769 49334 50678 51343 53185 55663 57862 60000]
        yy        (1,25) double = [3122 3121.9 3121.8 3121.7 3121.6 3121.5 3121.4 3121.3 3121.2 3121.1 3121 2440 1964 1465 976 698 466 335 232 181 154 103 51 20 0]
        
        Kzero     (1,1) double = 0.0296
        AirGrad   (1,1) double = 41000
        
        % Команды (логические флаги кнопок)
        CmdStart  (1,1) logical = false
        CmdZero   (1,1) logical = false
        CmdAirGrad    (1,1) logical = false

        CmdAirCurrent  (1,1) logical = false
        CmdRsrv2  (1,1) logical = false
        CmdRsrv3  (1,1) logical = false
        CmdRsrv4  (1,1) logical = false
    end
    
    methods
        function array = toArray(obj)
            % Динамический сборщик: сканирует свойства класса FromApp 
            % и собирает их в один плоский вектор типа double для UDP.
            meta = metaclass(obj);
            propNames = {meta.PropertyList.Name};
            
            array = [];
            for i = 1:length(propNames)
                % Забираем значение, приводим к double и вытягиваем в строку
                val = double(obj.(propNames{i}));
                array = [array, val(:)']; 
            end
        end
    end
    
    methods (Static)
        function types = getSimulinkTypes()
            % Возвращает типы данных для портов блока Byte Unpack
            meta = ?FromApp;
            propCount = length(meta.PropertyList);
            types = repmat({'double'}, 1, propCount);
        end
        
        function dims = getSimulinkDimensions()
            % Автоматически сканирует свойства класса и задает размерность 
            % выходных портов блока Byte Unpack (скаляры или векторы по 25 элементов)
            meta = ?FromApp;
            props = meta.PropertyList;
            dims = cell(1, length(props));
            for i = 1:length(props)
                dims{i} = props(i).Validation.Size; 
            end
        end

        function bytes = getRequiredBufferSize()
            % Прямой и надежный расчет без тяжелой рефлексии метаклассов.
            % Гарантирует возвращение чистого числового значения double.
            
            numScalars = 5;  % Amplitude, Periods, Sharpness
            numVectors = 2;  % xx, yy
            vecLength  = 25; % Длина каждого вектора
            numButtons = 7;  % CmdStart, CmdZero, CmdAir
            
            totalElements = numScalars + (numVectors * vecLength) + numButtons;
            bytes = double(totalElements * 8); % Явно приводим к double
        end
    end
end