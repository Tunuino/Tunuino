/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.calibration.CalibrationMap;
import java.util.Locale;
import javafx.beans.property.ReadOnlyStringWrapper;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.geometry.Insets;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.control.TextField;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;
import javafx.stage.Window;

/** Read-only map inspection. No calibration command is sent to a device. */
public final class CalibrationViewer {
    private CalibrationViewer() {}

    public static void show(Window owner, String filename, CalibrationMap map) {
        Stage stage = new Stage();
        stage.initOwner(owner);
        stage.setTitle("Tunuino · " + filename);
        TableView<ObservableList<String>> table = new TableView<>();
        table.setEditable(false);
        TableColumn<ObservableList<String>, String> load = column("MAP (kPa) / RPM", 0);
        load.setPrefWidth(150);
        table.getColumns().add(load);
        for (int i = 0; i < map.columns(); ++i) {
            table.getColumns().add(column(Integer.toString(map.rpmAt(i)), i + 1));
        }
        for (int j = 0; j < map.rows(); ++j) {
            ObservableList<String> row = FXCollections.observableArrayList();
            row.add(String.format(Locale.ROOT, "%.3f", map.loadKpaAt(j)));
            for (int i = 0; i < map.columns(); ++i) {
                row.add(String.format(Locale.ROOT, "%.3f", map.cell(j, i)));
            }
            table.getItems().add(row);
        }
        Label title = new Label(map.kind() + " · " + map.columns() + " × " + map.rows() + " · consulta offline");
        TextField rpm = new TextField(Integer.toString(map.rpmAt(0)));
        TextField kpa = new TextField(Double.toString(map.loadKpaAt(0)));
        rpm.setPrefColumnCount(8); kpa.setPrefColumnCount(8);
        Button evaluate = new Button("Calcular ponto");
        Label result = new Label("Introduz RPM e MAP para consultar o valor interpolado.");
        result.setWrapText(true);
        evaluate.setOnAction(event -> {
            try {
                var value = map.lookup(Double.parseDouble(rpm.getText().trim()), Double.parseDouble(kpa.getText().trim()));
                result.setText(String.format(Locale.ROOT, "%.3f · %s%s", value.value(), map.kind(),
                    value.clamped() ? " · fora dos eixos, valor limitado à borda" : ""));
            } catch (IllegalArgumentException e) {
                result.setText("RPM e MAP inválidos. Usa números não negativos com ponto decimal.");
            }
        });
        VBox header = new VBox(10, title, new HBox(8, new Label("RPM"), rpm,
            new Label("MAP (kPa)"), kpa, evaluate), result);
        header.setPadding(new Insets(12));
        BorderPane root = new BorderPane(table);
        root.setTop(header);
        stage.setScene(new Scene(root, 1080, 640));
        stage.show();
    }

    private static TableColumn<ObservableList<String>, String> column(String title, int index) {
        TableColumn<ObservableList<String>, String> column = new TableColumn<>(title);
        column.setCellValueFactory(cell -> new ReadOnlyStringWrapper(cell.getValue().get(index)));
        column.setSortable(false);
        column.setPrefWidth(85);
        return column;
    }
}
