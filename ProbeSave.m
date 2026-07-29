function ProbeSave(app, ColumnName)

% 1. Открываем окно сохранения файла
[file, path] = uiputfile('*.xlsx', 'Сохранить таблицу как...');

if ischar(file)
    fullFileName = fullfile(path, file);

    % 2. Формируем и сохраняем основную таблицу (LUT_data)
    T = array2table(app.LUT_data);
    screenHeaders = ColumnName;
    if length(screenHeaders) == size(app.LUT_data, 2)
        T.Properties.VariableNames = screenHeaders;
    end
    writetable(T, fullFileName);

    % 3. ВЫТЯГИВАЕМ ПАРАМЕТРЫ НАПРЯМУЮ ИЗ СВОЙСТВ APP
    paramsCell = {
        'ProbeType' , app.Settings.ProbeType;
        'Amplitude', app.Settings.Amplitude;
        'Periods',   app.Settings.Periods;
        'Sharpness', app.Settings.Sharpness;
        'Kzero',     app.Settings.Kzero;
        'AirGrad',   app.Settings.AirGrad
        };

    % 4. Записываем параметры в Excel, начиная с ячейки E2
    writecell(paramsCell, fullFileName, 'Range', 'E2');

    uialert(app.UIFigure, 'Данные таблицы и все параметры успешно сохранены!', 'Сохранение', 'Icon', 'success');
end
