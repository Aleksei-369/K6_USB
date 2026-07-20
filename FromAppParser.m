classdef FromAppParser < matlab.System
    % FromAppParser: Полностью автоматический бинарный маппинг в шину
    % Чистая индексация ячеек через маску блока БЕЗ evalin и БЕЗ динамических имен!

    properties(Nontunable)
        % Сюда Симулинк сам передаст FromApp_Template через маску блока сверху!
        TemplateStructure = struct();
    end

    properties(Access = private)
        % Локальный кэш для быстрой рантайм-сборки
        FieldNames
        CellTemplate
        NumFields
    end

    methods(Access = protected)
        function setupImpl(obj)
            % 🌟 ВЫПОЛНЯЕТСЯ ВСЕГО 1 РАЗ НА СТАРТЕ СИМУЛЯЦИИ!
            % Никаких evalin и сторонних вызовов. Работаем с переданным параметром.
            obj.FieldNames   = fieldnames(obj.TemplateStructure);
            
            % Схлопываем входную структуру в массив ячеек для легальной индексации c{i}
            obj.CellTemplate = struct2cell(obj.TemplateStructure);
            
            % Фиксируем количество полей в чистый, понятный системе double-скаляр
            obj.NumFields    = double(length(obj.FieldNames));
        end

        function y = stepImpl(obj, u)
            % u — сырой вектор uint8 из UDP Receive (наш 504-байтовый пакет)
            
            % Мгновенно перерезаем память uint8 в плоский вектор double силами ОЗУ
            v = typecast(u, 'double');
            
            % Забираем кэшированные шаблоны из памяти класса
            currentCell = obj.CellTemplate;
            fields      = obj.FieldNames;
            nFields     = obj.NumFields;
            
            offset = 0;
            for i = 1:nFields
                % Автоматически считываем количество элементов в текущей ячейке
                % Для скаляра = 1, для опорных векторов xx или yy = 25!
                num_elements = numel(currentCell{i});
                
                % Нарезаем плоский double-вектор v и восстанавливаем размерность ячейки
                currentCell{i} = reshape(v(offset+1 : offset+num_elements), size(currentCell{i}));
                
                % Сдвигаем указатель памяти для следующей ячейки
                offset = offset + num_elements;
            end
            
            % 🌟 Склеиваем ячейки обратно в готовую структуру нашей шины!
            y = cell2struct(currentCell, fields, 1);
        end
        
        % Настраиваем интерфейсные параметры портов Симулинка
        function num = getNumOutputsImpl(~)
            num = 1;
        end
        function out = getOutputDataTypeImpl(~)
            out = 'Bus: FromAppBus_Type'; 
        end
        function out = getOutputSizeImpl(~)
            out = [1 1];
        end
        function out = isOutputFixedSizeImpl(~)
            out = true;
        end
    end
end