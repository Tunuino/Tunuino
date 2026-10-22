/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.calibration.CalibrationDocument;
import java.io.File;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Path;
import java.util.Locale;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import javafx.beans.property.ReadOnlyStringWrapper;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.geometry.Insets;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Alert;
import javafx.scene.control.ButtonBar;
import javafx.scene.control.ButtonType;
import javafx.scene.control.Label;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.control.TextField;
import javafx.scene.control.cell.TextFieldTableCell;
import javafx.concurrent.Task;
import javafx.event.Event;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;
import javafx.stage.Window;
import javafx.stage.FileChooser;
import javafx.stage.WindowEvent;

/** Offline editing over immutable snapshots. */
public final class CalibrationViewer {
    private static final Set<Editor> OPEN_EDITORS = new LinkedHashSet<>();
    private CalibrationViewer() {}

    public static void show(Window owner, String filename, CalibrationMap map) {
        new Editor(owner, filename, map).show();
    }

    /** Called on the JavaFX thread before closing the main Console window. */
    public static boolean closeAll() {
        for (Editor editor : List.copyOf(OPEN_EDITORS)) {
            WindowEvent request = new WindowEvent(editor.stage, WindowEvent.WINDOW_CLOSE_REQUEST);
            Event.fireEvent(editor.stage, request);
            if (!request.isConsumed() && editor.stage.isShowing()) { editor.stage.close(); }
            if (editor.stage.isShowing()) {
                editor.stage.toFront();
                return false;
            }
        }
        return true;
    }

    private static final class Editor {
        private final Stage stage = new Stage();
        private final CalibrationDocument document;
        private final String originalFilename;
        private final TableView<ObservableList<String>> table = new TableView<>();
        private final Label state = new Label();
        private final Label result = new Label("Duplo clique para editar; Enter confirma e Esc cancela.");
        private final Button undo = new Button("Desfazer");
        private final Button redo = new Button("Refazer");
        private final Button save = new Button("Guardar cópia");
        private final ExecutorService writer = Executors.newSingleThreadExecutor(r -> new Thread(r, "tunuino-map-save"));
        private boolean saving;
        private boolean allowClose;

        Editor(Window owner, String filename, CalibrationMap map) {
            originalFilename = filename;
            document = new CalibrationDocument(map);
            stage.initOwner(owner);
            ConsoleBranding.applyIcon(stage);
            table.setEditable(true);
            TableColumn<ObservableList<String>, String> load = column("MAP (kPa) / RPM", 0);
            load.setPrefWidth(150);
            load.setEditable(false);
            table.getColumns().add(load);
            for (int i = 0; i < map.columns(); ++i) {
                int columnIndex = i;
                TableColumn<ObservableList<String>, String> value = column(Integer.toString(map.rpmAt(i)), i + 1);
                value.setCellFactory(TextFieldTableCell.forTableColumn());
                value.setOnEditCommit(event -> {
                    try {
                        document.editCell(event.getTablePosition().getRow(), columnIndex, event.getNewValue());
                        result.setText("Célula confirmada. Usa Guardar cópia para guardar o mapa.");
                    } catch (IllegalArgumentException e) {
                        result.setText(e.getMessage());
                    }
                    refreshRows();
                });
                table.getColumns().add(value);
            }
            table.editingCellProperty().addListener((property, oldValue, newValue) -> refreshControls());
            undo.setOnAction(event -> { document.undo(); result.setText("Alteração desfeita."); refreshRows(); });
            redo.setOnAction(event -> { document.redo(); result.setText("Alteração refeita."); refreshRows(); });
            save.setOnAction(event -> chooseSave(() -> {}));
            TextField rpm = new TextField(Integer.toString(map.rpmAt(0)));
            TextField kpa = new TextField(Double.toString(map.loadKpaAt(0)));
            rpm.setPrefColumnCount(8); kpa.setPrefColumnCount(8);
            Button evaluate = new Button("Calcular ponto");
            evaluate.setOnAction(event -> {
                try {
                    var value = document.snapshot().lookup(Double.parseDouble(rpm.getText().trim()), Double.parseDouble(kpa.getText().trim()));
                    result.setText(String.format(Locale.ROOT, "%.3f · %s%s", value.value(), map.kind(),
                        value.clamped() ? " · fora dos eixos, valor limitado à borda" : ""));
                } catch (IllegalArgumentException e) {
                    result.setText("RPM e MAP inválidos. Usa números não negativos com ponto decimal.");
                }
            });
            result.setWrapText(true);
            VBox header = new VBox(10,
                new Label(map.kind() + " · " + map.columns() + " × " + map.rows() + " · edição offline"),
                new HBox(8, undo, redo, save, state),
                new HBox(8, new Label("RPM"), rpm, new Label("MAP (kPa)"), kpa, evaluate), result);
            header.setPadding(new Insets(12));
            BorderPane root = new BorderPane(table);
            root.setTop(header);
            stage.setScene(new Scene(root, 1080, 680));
            stage.setOnCloseRequest(event -> {
                if (allowClose) { return; }
                if (saving || table.getEditingCell() != null) {
                    event.consume();
                    result.setText(saving ? "Aguarda a conclusão da gravação."
                        : "Confirma a célula com Enter ou cancela com Esc antes de fechar.");
                    return;
                }
                if (!document.isModified()) { return; }
                event.consume();
                ButtonType keep = new ButtonType("Guardar cópia", ButtonBar.ButtonData.YES);
                ButtonType discard = new ButtonType("Descartar", ButtonBar.ButtonData.NO);
                ButtonType cancel = new ButtonType("Cancelar", ButtonBar.ButtonData.CANCEL_CLOSE);
                Alert prompt = new Alert(Alert.AlertType.CONFIRMATION,
                    "O mapa tem alterações por guardar.", keep, discard, cancel);
                prompt.initOwner(stage);
                prompt.setHeaderText("Guardar alterações antes de fechar?");
                ButtonType choice = prompt.showAndWait().orElse(cancel);
                if (choice == keep) {
                    chooseSave(() -> { allowClose = true; stage.close(); });
                } else if (choice == discard) {
                    allowClose = true; stage.close();
                }
            });
            stage.setOnHidden(event -> { OPEN_EDITORS.remove(this); writer.shutdown(); });
            refreshRows();
        }

        void show() { OPEN_EDITORS.add(this); stage.show(); }

        private void refreshRows() {
            CalibrationMap map = document.snapshot();
            table.getItems().clear();
            for (int j = 0; j < map.rows(); ++j) {
                ObservableList<String> row = FXCollections.observableArrayList();
                row.add(String.format(Locale.ROOT, "%.3f", map.loadKpaAt(j)));
                for (int i = 0; i < map.columns(); ++i) {
                    row.add(String.format(Locale.ROOT, "%.3f", map.cell(j, i)));
                }
                table.getItems().add(row);
            }
            refreshControls();
        }

        private void refreshControls() {
            boolean editing = table.getEditingCell() != null;
            undo.setDisable(saving || editing || !document.canUndo());
            redo.setDisable(saving || editing || !document.canRedo());
            save.setDisable(saving || editing);
            table.setDisable(saving);
            state.setText(saving ? "A guardar…" : document.isModified() ? "Alterações por guardar" : "Sem alterações pendentes");
            String filename = document.savedPath() == null ? originalFilename : document.savedPath().getFileName().toString();
            stage.setTitle("Tunuino · " + filename + (document.isModified() ? " *" : ""));
        }

        private void chooseSave(Runnable afterSave) {
            FileChooser chooser = new FileChooser();
            chooser.setTitle("Guardar nova cópia do mapa");
            chooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Mapa Tunuino", "*.tnmap"));
            String base = document.savedPath() == null ? originalFilename : document.savedPath().getFileName().toString();
            if (base.toLowerCase(Locale.ROOT).endsWith(".tnmap")) { base = base.substring(0, base.length() - 6); }
            chooser.setInitialFileName(base + "-editado.tnmap");
            File selected = chooser.showSaveDialog(stage);
            if (selected == null) { return; }
            Path chosen = selected.toPath();
            Path destination = selected.getName().toLowerCase(Locale.ROOT).endsWith(".tnmap") ? chosen
                : chosen.resolveSibling(selected.getName() + ".tnmap");
            saving = true;
            refreshControls();
            Task<Void> task = new Task<>() {
                @Override protected Void call() throws Exception { document.saveAsNew(destination); return null; }
            };
            task.setOnSucceeded(event -> {
                saving = false;
                result.setText("Cópia guardada em " + destination);
                refreshControls();
                afterSave.run();
            });
            task.setOnFailed(event -> {
                saving = false;
                result.setText(task.getException() instanceof FileAlreadyExistsException
                    ? "O ficheiro já existe. Escolhe outro nome."
                    : "Não foi possível guardar: " + task.getException().getMessage());
                refreshControls();
            });
            writer.submit(task);
        }
    }

    private static TableColumn<ObservableList<String>, String> column(String title, int index) {
        TableColumn<ObservableList<String>, String> column = new TableColumn<>(title);
        column.setCellValueFactory(cell -> new ReadOnlyStringWrapper(cell.getValue().get(index)));
        column.setSortable(false);
        column.setReorderable(false);
        column.setPrefWidth(85);
        return column;
    }
}
