/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.maintenance.BuildInfo;
import io.tunuino.protocol.DeviceIdentity;
import java.util.LinkedHashMap;
import java.util.Map;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.control.Label;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.ToggleButton;
import javafx.scene.control.ToggleGroup;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.FlowPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.Region;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;
import javafx.scene.shape.SVGPath;

/** Shared application chrome; each navigation entry opens an implemented workspace. */
final class ConsoleShell extends BorderPane {
    private record Page(String title, String description, Node content) {}
    private final Map<String, Page> pages = new LinkedHashMap<>();
    private final Map<String, ToggleButton> navigation = new LinkedHashMap<>();
    private final Label heading = new Label();
    private final Label description = new Label();
    private final Label connection = new Label("●  OFFLINE");
    private final Label device = new Label("Nenhuma ECU identificada");
    private final BorderPane workspace = new BorderPane();
    private final VBox sidebar = new VBox(8);
    private final ToggleGroup section = new ToggleGroup();

    ConsoleShell(Node hardware, Node connectionControls, Label status, Label identity) {
        getStyleClass().add("console-shell");
        ImageView logo = new ImageView(new Image(ConsoleShell.class.getResource("tunuino.png").toExternalForm()));
        logo.setFitWidth(172); logo.setFitHeight(58); logo.setPreserveRatio(true);
        StackPane logoBox = new StackPane(logo); logoBox.getStyleClass().add("brand-logo");
        Label product = new Label("CONSOLE"); product.getStyleClass().add("eyebrow");
        VBox productName = new VBox(3, product, device);
        device.getStyleClass().add("muted");
        Region gap = new Region(); HBox.setHgrow(gap, Priority.ALWAYS);
        connection.getStyleClass().addAll("connection-badge", "offline");
        HBox masthead = new HBox(22, logoBox, productName, gap, new Label("Centralina"), hardware, connection);
        masthead.setAlignment(Pos.CENTER_LEFT); masthead.getStyleClass().add("masthead");
        VBox header = new VBox(masthead, connectionControls); header.getStyleClass().add("app-header");
        setTop(header);
        sidebar.getStyleClass().add("sidebar"); sidebar.setPrefWidth(190); sidebar.setMinWidth(190);
        Label menu = new Label("ÁREA DE TRABALHO"); menu.getStyleClass().add("sidebar-caption");
        sidebar.getChildren().add(menu);
        Region stretch = new Region(); VBox.setVgrow(stretch, Priority.ALWAYS);
        Label build = new Label(BuildInfo.label()); build.getStyleClass().add("build-label"); build.setWrapText(true);
        Label local = new Label("TUNUINO\nFerramentas locais · USB"); local.getStyleClass().add("sidebar-footer");
        sidebar.getChildren().addAll(stretch, local, build); setLeft(sidebar);
        heading.getStyleClass().add("page-heading"); description.getStyleClass().add("page-description");
        description.setWrapText(true);
        VBox pageHeader = new VBox(5, heading, description); pageHeader.getStyleClass().add("page-header");
        workspace.setTop(pageHeader); workspace.getStyleClass().add("workspace"); setCenter(workspace);
        status.getStyleClass().add("status-message"); identity.getStyleClass().add("identity-message");
        status.setWrapText(true); identity.setWrapText(true);
        VBox footer = new VBox(3, status, identity); footer.getStyleClass().add("status-bar");
        status.setId("console-status");
        setBottom(footer);
        sceneProperty().addListener((property, before, scene) -> {
            if (scene == null) return;
            java.util.function.DoubleConsumer resize = height -> {
                boolean compact = height < 800;
                pseudoClassStateChanged(javafx.css.PseudoClass.getPseudoClass("compact"), compact);
                logo.setFitHeight(compact ? 40 : 58);
                logo.setFitWidth(compact ? 142 : 172);
            };
            scene.heightProperty().addListener((observable, oldHeight, height) -> resize.accept(height.doubleValue()));
            resize.accept(scene.getHeight());
        });
    }

    void addPage(String key, String title, String detail, String iconPath, Node content) {
        pages.put(key, new Page(title, detail, content));
        SVGPath icon = new SVGPath(); icon.setContent(iconPath); icon.getStyleClass().add("navigation-icon");
        StackPane glyph = new StackPane(icon); glyph.setMinSize(22, 22); glyph.setPrefSize(22, 22);
        ToggleButton button = new ToggleButton(key, glyph); button.setMaxWidth(Double.MAX_VALUE);
        button.getStyleClass().add("navigation-button"); button.setToggleGroup(section);
        button.setId("navigate-" + navigation.size());
        button.setOnAction(event -> select(key));
        navigation.put(key, button); sidebar.getChildren().add(sidebar.getChildren().size() - 3, button);
        if (pages.size() == 1) select(key);
    }

    void select(String key) {
        Page page = pages.get(key);
        heading.setText(page.title()); description.setText(page.description());
        workspace.setCenter(page.content()); navigation.get(key).setSelected(true);
    }

    void connectionState(DeviceIdentity identity, boolean connected) {
        connection.setText(identity != null ? "●  ECU IDENTIFICADA" : connected ? "●  USB LIGADO" : "●  OFFLINE");
        connection.getStyleClass().removeAll("online", "offline");
        connection.getStyleClass().add(identity != null ? "online" : "offline");
        device.setText(identity == null ? "Nenhuma ECU identificada"
            : identity.family() + " · " + identity.mcuSeries() + " · firmware " + identity.firmwareVersion());
    }

    static Label caption(String text) { Label label = new Label(text); label.getStyleClass().add("eyebrow"); return label; }

    static FlowPane actions(Node... controls) {
        FlowPane row = new FlowPane(8, 8, controls); row.getStyleClass().add("actions"); return row;
    }

    static VBox card(String title, String detail, Node... controls) {
        Label explanation = new Label(detail); explanation.getStyleClass().add("muted"); explanation.setWrapText(true);
        VBox panel = new VBox(12, caption(title), explanation);
        panel.getChildren().addAll(controls); panel.getStyleClass().add("panel"); return panel;
    }

    static ScrollPane scroll(Node content) {
        ScrollPane scroll = new ScrollPane(content); scroll.setFitToWidth(true);
        scroll.setHbarPolicy(ScrollPane.ScrollBarPolicy.NEVER); scroll.getStyleClass().add("workspace-scroll");
        return scroll;
    }
}
