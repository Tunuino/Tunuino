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
    private final VBox navItems = new VBox(5);
    private final ToggleGroup section = new ToggleGroup();
    private DeviceIdentity shownIdentity;
    private Boolean shownConnected;

    void group(String title, String... keys) {
        VBox entries=new VBox(3);
        for(String key:keys){ToggleButton item=navigation.get(key);if(item==null)throw new IllegalArgumentException(key);
            navItems.getChildren().remove(item);entries.getChildren().add(item);}
        javafx.scene.control.TitledPane group=new javafx.scene.control.TitledPane(title,entries);
        group.setExpanded(true);group.getStyleClass().add("navigation-group");navItems.getChildren().add(group);
    }

    void label(String key,String text){navigation.get(key).setText(text);}

    ConsoleShell(Node hardware, Node connectionControls, Label status, Label identity) {
        getStyleClass().add("console-shell");
        ImageView logo=new ImageView(new Image(java.util.Objects.requireNonNull(
            ConsoleShell.class.getResource("tunuino.png"),"Missing Tunuino logo").toExternalForm()));
        logo.setId("console-logo");logo.setPreserveRatio(true);logo.setSmooth(true);
        logo.setFitWidth(280);logo.setFitHeight(74);
        Label subtype=new Label("Console");subtype.getStyleClass().add("wordmark-subtitle");
        Label build=new Label(BuildInfo.label());build.setId("console-active-build");build.getStyleClass().add("build-label");
        HBox brandingDetails=new HBox(12,subtype,build);brandingDetails.setAlignment(Pos.CENTER_LEFT);
        VBox productName=new VBox(0,logo,brandingDetails);productName.setMinWidth(280);productName.setPrefWidth(280);
        device.getStyleClass().add("muted");device.setWrapText(true);device.setMaxWidth(210);
        connection.getStyleClass().addAll("connection-badge","offline");
        VBox state=new VBox(3,connection,device);state.setMinWidth(180);state.setMaxWidth(215);
        VBox controls=new VBox(2,connectionControls);controls.setMinWidth(280);controls.setPrefWidth(640);HBox.setHgrow(controls,Priority.ALWAYS);
        HBox masthead=new HBox(12,productName,state,controls);masthead.setAlignment(Pos.CENTER_LEFT);masthead.getStyleClass().add("masthead");
        // Hardware selection stays available beside the USB controls.
        if(connectionControls instanceof FlowPane bar)bar.getChildren().add(0,hardware);
        javafx.scene.control.MenuBar menus=new javafx.scene.control.MenuBar();menus.setId("console-menu-bar");
        String[][] entries={{"Ficheiro","ECU Project","Tuning","Data Log"},{"ECU","Inputs / Outputs","Engine","Output Tests"},
            {"Ver","Dashboard","Fuel Tables","Ignition Tables"},{"Ferramentas","Firmware","Diagnostics"},{"Ajuda","Settings"}};
        for(String[] entry:entries){var menuEntry=new javafx.scene.control.Menu(entry[0]);for(int i=1;i<entry.length;i++){String key=entry[i];var item=new javafx.scene.control.MenuItem(key);item.setOnAction(e->select(key));menuEntry.getItems().add(item);}menus.getMenus().add(menuEntry);}
        VBox header=new VBox(menus,masthead);header.getStyleClass().add("app-header");setTop(header);
        sidebar.getStyleClass().add("sidebar");
        sidebar.setPrefWidth(205); sidebar.setMinWidth(205); sidebar.setMinHeight(0);
        Label menu = new Label("ÁREA DE TRABALHO"); menu.getStyleClass().add("sidebar-caption");
        ScrollPane navigationScroll=new ScrollPane(navItems);
        navigationScroll.setFitToWidth(true);
        navigationScroll.setHbarPolicy(ScrollPane.ScrollBarPolicy.NEVER);
        navigationScroll.setVbarPolicy(ScrollPane.ScrollBarPolicy.AS_NEEDED);
        navigationScroll.setMinHeight(0);
        navigationScroll.getStyleClass().add("sidebar-scroll");
        VBox.setVgrow(navigationScroll, Priority.ALWAYS);
        Label sidebarBuild = new Label(BuildInfo.label()); sidebarBuild.getStyleClass().add("build-label");
        Label local = new Label("TUNUINO\nFerramentas locais · USB"); local.getStyleClass().add("sidebar-footer");
        sidebar.getChildren().addAll(navigationScroll,sidebarBuild);
        setLeft(sidebar);
        heading.getStyleClass().add("page-heading"); description.getStyleClass().add("page-description");
        description.setWrapText(true);
        VBox pageHeader = new VBox(5, heading, description); pageHeader.getStyleClass().add("page-header");
        workspace.setTop(pageHeader); workspace.getStyleClass().add("workspace");
        // Scrollable workspaces must shrink below their content's preferred size
        // when the window is resized, rather than pushing the status bar offscreen.
        workspace.setMinHeight(0); workspace.setMinWidth(0);
        setCenter(workspace);
        status.getStyleClass().add("status-message"); identity.getStyleClass().add("identity-message");
        status.setWrapText(true); identity.setWrapText(true);
        VBox footer = new VBox(3, status); footer.getStyleClass().add("status-bar");
        status.setId("console-status");
        setBottom(footer);
        sceneProperty().addListener((property, before, scene) -> {
            if (scene == null) return;
            java.util.function.DoubleConsumer resize = height -> {
                boolean compact = height < 800;
                boolean shortWindow = height < 700;
                pseudoClassStateChanged(javafx.css.PseudoClass.getPseudoClass("compact"), compact);
                pseudoClassStateChanged(javafx.css.PseudoClass.getPseudoClass("short"), shortWindow);

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
        navigation.put(key, button); navItems.getChildren().add(button);
        if (pages.size() == 1) select(key);
    }

    void finishNavigation(){
        for(var button:navigation.values())if(button.getParent() instanceof javafx.scene.layout.Pane parent)parent.getChildren().remove(button);
        navItems.getChildren().clear();
        for(String key:java.util.List.of("Dashboard","Tuning","Fuel Tables","Ignition Tables","Autotune","Data Log","Inputs / Outputs","Diagnostics","Firmware","Settings"))navItems.getChildren().add(navigation.get(key));
        VBox extra=new VBox(2);for(String key:java.util.List.of("Engine","IO Functions","Aux Control","Fuel Features","Output Tests","ECU Project"))extra.getChildren().add(navigation.get(key));
        var group=new javafx.scene.control.TitledPane("Configuração do motor",extra);group.getStyleClass().add("navigation-group");group.setExpanded(false);navItems.getChildren().add(group);
    }
    void select(String key) {
        Page page = pages.get(key);
        heading.setText(page.title()); description.setText(page.description());
        workspace.setCenter(page.content()); navigation.get(key).setSelected(true);
        boolean cockpit=key.equals("Dashboard");workspace.getTop().setVisible(!cockpit);workspace.getTop().setManaged(!cockpit);
        pseudoClassStateChanged(javafx.css.PseudoClass.getPseudoClass("cockpit"),cockpit);
    }

    void connectionState(DeviceIdentity identity, boolean connected) {
        if(shownConnected!=null&&shownConnected==connected&&java.util.Objects.equals(shownIdentity,identity))return;
        shownConnected=connected;shownIdentity=identity;
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
        scroll.setMinHeight(0);scroll.setMinWidth(0);
        scroll.setHbarPolicy(ScrollPane.ScrollBarPolicy.NEVER); scroll.getStyleClass().add("workspace-scroll");
        return scroll;
    }
}
