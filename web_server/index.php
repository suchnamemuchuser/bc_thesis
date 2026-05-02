<?php require_once 'config.php'; ?>

<html>
    <head>
        <link rel="stylesheet" href="style.css">
        <title>RT2 control</title>
    </head>
    <body>
        <?php $pageTitle = "Observation Planner"; include 'header.php'; ?>
        <section id="form">
            <form id="plan-form">
                <div id="plan-date">
                    <label for="date">Date:</label>
                    <input type="date" id="date" name="date" value="<?php echo date('Y-m-d'); ?>" min="<?php echo date('Y-m-d'); ?>" required>
                </div>
                
                <h3>Observation Targets</h3>
                <table id="targets-table">
                    <thead>
                        <tr>
                            <th>Target Name</th>
                            <th>Database</th>
                            <th>Calculate</th>
                            <th>Action</th>
                        </tr>
                    </thead>
                    <tbody id="targets-body">
                        <?php foreach ($predefinedTargets as $index => $target): ?>
                            <tr>
                                <td><input type="text" name="targets[<?php echo $index; ?>][name]" value="<?php echo htmlspecialchars($target['name']); ?>" readonly></td>
                                <td>
                                    <select name="targets[<?php echo $index; ?>][type]">
                                        <option value="solar" <?php echo $target['type'] === 'solar' ? 'selected' : 'disabled'; ?>>Solar System</option>
                                        <option value="deep" <?php echo $target['type'] === 'deep' ? 'selected' : 'disabled'; ?>>Deep Space</option>
                                    </select>
                                </td>
                                <td><input type="checkbox" name="targets[<?php echo $index; ?>][enabled]" <?php echo $target['enabled'] ? 'checked' : ''; ?>></td>
                                <td><small>(Predefined)</small></td>
                            </tr>
                        <?php endforeach; ?>
                    </tbody>
                </table>
                
                <div class="buttons">
                    <button type="button" onclick="addRow()">+ Add Custom Target</button>
                    <button type="submit" id="submit-btn">Generate Visibility</button>
                </div>
            </form>
            <div>
                <img alt="popisek" src="<?php echo $radarField ?>">            
            </div>
        </section>

        <section id="visibility">
            <h2>Visibility Windows</h3>
            <ul id="windows-list"></ul>
            <img id="plan-graph" class="graph" src="" alt="Visibility" style="display: none">
        </section>

        <section id="observations">
            <h2>Planned Observations</h3>
            <img id="timeline-graph" class="graph" src="" alt="Observation Plan">

            <table id="plan-table">
                <thead>
                    <tr>
                        <th>Target</th>
                        <th>Id</th>
                        <th>OST</th>
                        <th>RST</th>
                        <th>ET</th>
                        <th>Action</th>
                    </tr>
                </thead>
                <tbody id="plan-table-body"></tbody>
            </table>
        </section>

        <script src="script.js"></script>
    </body>
</html>
