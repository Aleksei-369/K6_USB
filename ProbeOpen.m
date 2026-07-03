function ProbeOpen(app)
% 1. Открываем системное окно для выбора файла Excel
[file, path] = uigetfile('*.xlsx', 'Выберите файл Excel для загрузки...');

% 2. Если пользователь выбрал файл и нажал "Открыть"
if ischar(file)
    fullFileName = fullfile(path, file);

    try
        % 3. Читаем из Excel только числовую матрицу
        % (функция сама поймет, что первую строчку с текстом нужно пропустить)
        m = readmatrix(fullFileName);

        % Проверяем, не пустой ли файл
        if isempty(m)
            uialert(app.UIFigure, 'Выбранный файл Excel не содержит числовых данных!', 'Ошибка');
            return;
        end

        % 4. Разкладываем данные
        H = m(:,1);
        N = m(:,2);
        H( isnan(H)) = [];
        N( isnan(N)) = [];

        app.LUT_data = num2cell( H);
        app.LUT_data(:, 2) = num2cell( N);
        app.LUT_data(:, 3) = num2cell( zeros( length(H), 1));

        app.Settings.Amplitude = m( 1, 6);
        app.Settings.Periods = m( 2, 6);
        app.Settings.Sharpness = m( 3, 6);
        app.Settings.Kzero = m( 4, 6);
        app.Settings.AirGrad = m( 5, 6);

        % Показываем сообщение об успехе
        uialert(app.UIFigure, 'Данные из Excel успешно загружены в приложение!', 'Успех', 'Icon', 'success');

    catch ME
        % Если файл поврежден или заблокирован, покажем ошибку
        uialert(app.UIFigure, ['Не удалось прочитать файл. Ошибка: ' ME.message], 'Ошибка');
    end
end