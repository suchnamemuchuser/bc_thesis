<html>
    <head>
        <link rel="stylesheet" href="style.css">
        <title>Welcome to your website!</title>
    </head>
    <body>
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
                <tr>
                    <td><input type="text" name="targets[0][name]" value="Sun" readonly></td>
                    <td>
                        <select name="targets[0][type]">
                            <option value="solar" selected>Solar System</option>
                            <option value="deep" disabled>Deep Space</option>
                        </select>
                    </td>
                    <td><input type="checkbox" name="targets[0][enabled]" checked></td>
                    <td><small>(Predefined)</small></td>
                </tr>
                <tr>
                    <td><input type="text" name="targets[1][name]" value="B0329+54" readonly></td>
                    <td>
                        <select name="targets[1][type]">
                            <option value="solar" disabled>Solar System</option>
                            <option value="deep" selected>Deep Space</option>
                        </select>
                    </td>
                    <td><input type="checkbox" name="targets[1][enabled]" checked></td>
                    <td><small>(Predefined)</small></td>
                </tr>
                <tr>
                    <td><input type="text" name="targets[2][name]" value="B0943+10" readonly></td>
                    <td>
                        <select name="targets[2][type]">
                            <option value="solar" disabled>Solar System</option>
                            <option value="deep" selected>Deep Space</option>
                        </select>
                    </td>
                    <td><input type="checkbox" name="targets[2][enabled]" checked></td>
                    <td><small>(Predefined)</small></td>
                </tr>
                <tr>
                    <td><input type="text" name="targets[3][name]" value="B1957+20" readonly></td>
                    <td>
                        <select name="targets[3][type]">
                            <option value="solar" disabled>Solar System</option>
                            <option value="deep" selected>Deep Space</option>
                        </select>
                    </td>
                    <td><input type="checkbox" name="targets[3][enabled]" checked></td>
                    <td><small>(Predefined)</small></td>
                </tr>
                <tr>
                    <td><input type="text" name="targets[4][name]" value="B1937+21" readonly></td>
                    <td>
                        <select name="targets[4][type]">
                            <option value="solar" disabled>Solar System</option>
                            <option value="deep" selected>Deep Space</option>
                        </select>
                    </td>
                    <td><input type="checkbox" name="targets[4][enabled]" checked></td>
                    <td><small>(Predefined)</small></td>
                </tr>
            </tbody>
        </table>
        
        <div style="margin-top: 10px;">
            <button type="button" onclick="addRow()">+ Add Custom Object</button>
        </div>
        
        <br><br>
        <button type="submit" id="submit-btn">Generate Visibility</button>
    </form>

    <div id="result-container" style="display: none">
        <h3>Visibility Windows</h3>
        <ul id="windows-list"></ul>
        <img id="plan-graph" src="" alt="Observation Plan">
    </div>

    <script src="script.js"></script>
    </body>
</html>
