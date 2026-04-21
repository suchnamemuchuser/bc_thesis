<?php require_once 'config.php'; ?>

<html>
    <head>
        <link rel="stylesheet" href="style.css">
        <title>RT2 control</title>
    </head>
    <body>
        <a href="history.php" style="text-decoration: none; color: #333;">&rarr; Planned Recordings</a>
        <h1>Pulsar Observation Planner</h1>

    <form id="plan-form">
        <label for="date">Date:</label>
        <input type="date" id="date" name="date" value="<?php echo date('Y-m-d'); ?>" min="<?php echo date('Y-m-d'); ?>" required>
        
        <h3>Observation Targets</h3>
        <table id="targets-table" style="width:450px; border: 1px solid #ccc; border-collapse: collapse;">
            <thead>
                <tr style="background: #f0f0f0;">
                    <th>Object Name</th>
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
        
        <div style="margin-top: 10px;">
            <button type="button" onclick="addRow()">+ Add Custom Object</button>
        </div>
        
        <br><br>
        <button type="submit" id="submit-btn">Generate Visibility</button>
    </form>

    <div id="result-container">
        <h3>Visibility Windows</h3>
        <ul id="windows-list"></ul>
        <img id="plan-graph" class="graph" src="" alt="Visibility" style="display: none">
        <img id="timeline-graph" class="graph" src="" alt="Observation Plan">

        <h3>Planned Observations</h3>
        <table id="plan-table" style="width:100%; border-collapse: collapse;">
            <thead>
                <tr style="background: #f0f0f0;">
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
    </div>

    <script src="script.js"></script>
    </body>
</html>
