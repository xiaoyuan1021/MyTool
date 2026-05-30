        // ===== Global Cleanup State =====
        let activeTimers = [];
        let currentEventSource = null;
        let statsFetchPending = false;

        function registerTimer(id) {
            activeTimers.push(id);
        }

        function clearAllTimers() {
            activeTimers.forEach(id => clearInterval(id));
            activeTimers = [];
        }

        function closeEventSource() {
            if (currentEventSource) {
                currentEventSource.close();
                currentEventSource = null;
            }
        }

        // ===== Time =====
        function updateTime() {
            const now = new Date();
            document.getElementById('headerTime').textContent = now.toLocaleString('zh-CN', { hour12: false });
        }
        updateTime();
        registerTimer(setInterval(updateTime, 1000));

        // ===== Charts =====
        let trendChart = null;
        let roiChart = null;
        let trendData = [];
        let roiData = {};

        function initCharts() {
            const trendCtx = document.getElementById('trendChart').getContext('2d');
            const roiCtx = document.getElementById('roiChart').getContext('2d');

            trendChart = new Chart(trendCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: '检测结果 (1=通过, 0=失败)',
                        data: [],
                        borderColor: '#22c55e',
                        backgroundColor: 'rgba(34, 197, 94, 0.08)',
                        fill: true,
                        tension: 0.4,
                        pointRadius: 3,
                        pointBackgroundColor: '#22c55e',
                        borderWidth: 2,
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    interaction: { intersect: false, mode: 'index' },
                    plugins: {
                        legend: { display: false },
                        tooltip: {
                            backgroundColor: '#1c2030',
                            borderColor: '#2a2f45',
                            borderWidth: 1,
                            titleColor: '#e8eaf0',
                            bodyColor: '#8b90a5',
                        },
                    },
                    scales: {
                        x: {
                            display: true,
                            ticks: { color: '#5a5f75', maxTicksLimit: 10, font: { size: 10 } },
                            grid: { color: 'rgba(42,47,69,0.3)' },
                        },
                        y: {
                            min: -0.1,
                            max: 1.1,
                            ticks: {
                                color: '#5a5f75',
                                font: { size: 10 },
                                callback: function(v) { return v === 0 ? 'FAIL' : v === 1 ? 'PASS' : ''; }
                            },
                            grid: { color: 'rgba(42,47,69,0.3)' },
                        }
                    },
                    animation: { duration: 300 },
                }
            });

            roiChart = new Chart(roiCtx, {
                type: 'doughnut',
                data: {
                    labels: [],
                    datasets: [{
                        data: [],
                        backgroundColor: [
                            '#22c55e', '#ef4444', '#3b82f6', '#f59e0b',
                            '#8b5cf6', '#06b6d4', '#ec4899', '#14b8a6'
                        ],
                        borderColor: '#1c2030',
                        borderWidth: 2,
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    cutout: '65%',
                    plugins: {
                        legend: {
                            position: 'right',
                            labels: {
                                color: '#8b90a5',
                                font: { size: 11 },
                                usePointStyle: true,
                                padding: 12,
                            }
                        },
                        tooltip: {
                            backgroundColor: '#1c2030',
                            borderColor: '#2a2f45',
                            borderWidth: 1,
                            callbacks: {
                                label: function(ctx) {
                                    const total = ctx.dataset.data.reduce((a, b) => a + b, 0);
                                    const pct = total > 0 ? (ctx.parsed / total * 100).toFixed(1) : 0;
                                    return ` ${ctx.label}: ${ctx.parsed} 次 (${pct}%)`;
                                }
                            }
                        },
                    },
                    animation: { duration: 300 },
                }
            });
        }

        // ===== Update Trend Chart =====
        function updateTrendChart(results) {
            const recent = results.slice(-50);
            const labels = recent.map((r, i) => {
                const d = new Date(r.timestamp || Date.now());
                return d.toLocaleTimeString('zh-CN', { hour12: false });
            });
            const data = recent.map(r => r.passed ? 1 : 0);

            trendChart.data.labels = labels;
            trendChart.data.datasets[0].data = data;
            trendChart.update('none');
        }

        // ===== Update ROI Chart =====
        function updateRoiChart(roiStats) {
            const entries = Object.entries(roiStats);
            if (entries.length === 0) return;

            // Sort by total desc, take top 8
            entries.sort((a, b) => b[1].total - a[1].total);
            const top = entries.slice(0, 8);

            roiChart.data.labels = top.map(([name]) => name.length > 10 ? name.slice(0, 10) + '..' : name);
            roiChart.data.datasets[0].data = top.map(([, v]) => v.total);
            roiChart.update('none');
        }

        // ===== Update ROI Stats Table =====
        function updateRoiStatsTable(roiStats) {
            const tbody = document.getElementById('roiStatsBody');
            const empty = document.getElementById('roiStatsEmpty');
            const entries = Object.entries(roiStats);

            if (entries.length === 0) {
                tbody.innerHTML = '';
                empty.style.display = 'block';
                return;
            }
            empty.style.display = 'none';
            entries.sort((a, b) => b[1].total - a[1].total);

            tbody.innerHTML = entries.map(([name, s]) => {
                const rate = s.total > 0 ? (s.passed / s.total * 100).toFixed(1) : 0;
                return `<tr>
                    <td>${name}</td>
                    <td>${s.total}</td>
                    <td style="color:var(--accent-green)">${s.passed}</td>
                    <td style="color:${s.failed > 0 ? 'var(--accent-red)' : 'var(--text-secondary)'}">${s.failed}</td>
                    <td><span class="${rate >= 90 ? 'badge-pass' : 'badge-fail'}">${rate}%</span></td>
                </tr>`;
            }).join('');
        }

        // ===== SSE =====
        let resultCount = 0;
        let initialResultsLoaded = false;

        function connectSSE() {
            // Close existing connection before creating new one
            closeEventSource();

            const evtSource = new EventSource('/stream');
            currentEventSource = evtSource;

            evtSource.addEventListener('connected', function() {
                console.log('[SSE] 已连接');
            });

            evtSource.addEventListener('mqtt_status', function(e) {
                const data = JSON.parse(e.data);
                const status = document.getElementById('mqttStatus');
                const dot = document.getElementById('mqttDot');
                const label = document.getElementById('mqttLabel');
                if (data.connected) {
                    status.className = 'mqtt-status online';
                    dot.className = 'mqtt-dot online';
                    label.textContent = 'MQTT 已连接';
                } else {
                    status.className = 'mqtt-status offline';
                    dot.className = 'mqtt-dot offline';
                    label.textContent = 'MQTT 已断开';
                }
            });

            evtSource.addEventListener('result', function(e) {
                const data = JSON.parse(e.data);
                addResultRow(data);
                console.log(`[DEBUG] SSE result 事件触发 updateStats`);
                updateStats();

                // Toast
                showToast(data);
            });

            evtSource.addEventListener('heartbeat', function(e) {
                const data = JSON.parse(e.data);
                fetchDevices();
            });

            evtSource.onerror = function() {
                console.log('[SSE] 连接断开，5秒后重连...');
                // Close before reconnecting
                closeEventSource();
                setTimeout(connectSSE, 5000);
            };
        }

        // ===== Add Result Row =====
        function addResultRow(data) {
            const tbody = document.getElementById('resultsBody');
            const empty = document.getElementById('resultsEmpty');
            empty.style.display = 'none';

            const time = data.dateTime ? data.dateTime.split('T')[1] || data.dateTime : '--';
            const shortTime = time.length > 8 ? time.slice(11, 19) : time;

            const badge = data.passed ?
                '<span class="badge-pass"><i class="fas fa-check"></i> PASS</span>' :
                '<span class="badge-fail"><i class="fas fa-times"></i> FAIL</span>';

            const barcodeIcon = data.hasBarcode ? '<i class="fas fa-qrcode" style="color:var(--accent-cyan)"></i>' : '--';

            const tr = document.createElement('tr');
            tr.className = 'row-new';
            tr.innerHTML = `
                <td style="font-size:12px;color:var(--text-secondary);font-variant-numeric:tabular-nums">${shortTime || '--'}</td>
                <td class="device-name" style="font-size:12px">${data.deviceId || '--'}</td>
                <td>${data.roiName || '全图'}</td>
                <td>${badge}</td>
                <td>${data.totalRegionCount ?? '--'}</td>
                <td>${barcodeIcon}</td>
            `;
            tbody.insertBefore(tr, tbody.firstChild);

            // Keep max 100 rows in DOM
            while (tbody.children.length > 100) {
                tbody.removeChild(tbody.lastChild);
            }

            resultCount++;
            document.getElementById('resultCount').textContent = resultCount + ' 条';

            // Remove highlight after animation
            setTimeout(() => { tr.className = ''; }, 1500);
        }

        // ===== Update Stats =====

        function updateStats() {
            if (!statsFetchPending) {
                statsFetchPending = true;
                setTimeout(() => {
                    console.log(`[DEBUG] updateStats → fetchStats + fetchTodayStats`);
                    fetchStats();
                    fetchTodayStats();
                    statsFetchPending = false;
                }, 500);
            }
        }

        function fetchStats() {
            fetch('/api/stats')
                .then(r => r.json())
                .then(s => {
                    console.log(`[DEBUG] fetchStats 更新主看板: total=${s.total}, passed=${s.passed}`);
                    document.getElementById('statTotal').textContent = s.total;
                    document.getElementById('statPassRate').textContent = s.passRate;
                    document.getElementById('statPassed').textContent = s.passed;
                    document.getElementById('statFailed').textContent = s.failed;
                    document.getElementById('statDevices').textContent = s.onlineCount;
                    document.getElementById('statTotalDevices').textContent = s.deviceCount;
                })
                .catch(err => console.error('[Stats] fetchStats failed:', err));
        }

        function fetchDevices() {
            fetch('/api/devices')
                .then(r => r.json())
                .then(devices => {
                    const list = document.getElementById('deviceList');
                    const countEl = document.getElementById('deviceCount');
                    countEl.textContent = devices.length + ' 台';

                    if (devices.length === 0) {
                        list.innerHTML = `
                            <div class="empty-state">
                                <i class="fas fa-plug"></i>
                                <p>暂无设备上线</p>
                            </div>`;
                        return;
                    }

                    list.innerHTML = devices.map(d => {
                        const status = d.status === 'online' ? 'online' : 'offline';
                        const statusLabel = d.status === 'online' ? '在线' : '离线';
                        const lastResult = d.last_result ? `
                            <span style="color:${d.last_result.passed ? 'var(--accent-green)' : 'var(--accent-red)'}">
                                ${d.last_result.passed ? 'PASS' : 'FAIL'} · ${d.last_result.roiName}
                            </span>` : '暂无数据';
                        return `
                            <div class="device-item">
                                <div class="device-info">
                                    <div class="device-icon"><i class="fas fa-microchip"></i></div>
                                    <div class="device-meta">
                                        <div class="name"><span class="device-dot ${status}"></span>${d.device_id}</div>
                                        <div class="detail">检测 ${d.result_count || 0} 次 · 最后: ${lastResult}</div>
                                    </div>
                                </div>
                                <span class="device-status-badge ${status}">${statusLabel}</span>
                            </div>`;
                    }).join('');
                });
        }

        function fetchResults() {
            fetch('/api/results/recent?limit=50')
                .then(r => r.json())
                .then(results => {
                    if (results.length > 0) {
                        document.getElementById('resultsEmpty').style.display = 'none';
                        updateTrendChart(results);

                        // 初始加载时填充结果表格
                        if (!initialResultsLoaded) {
                            initialResultsLoaded = true;
                            const tbody = document.getElementById('resultsBody');
                            tbody.innerHTML = '';
                            results.slice().reverse().forEach(r => {
                                addResultRow(r);
                            });
                        }
                    }
                });

            fetch('/api/stats/by-roi')
                .then(r => r.json())
                .then(roiStats => {
                    updateRoiChart(roiStats);
                    updateRoiStatsTable(roiStats);
                });
        }

        function fetchTodayStats() {
            fetch('/api/stats/today')
                .then(r => r.json())
                .then(s => {
                    document.getElementById('statToday').textContent = s.total;
                    document.getElementById('statTodayLabel').textContent =
                        `合格 ${s.passed} / 不合格 ${s.failed}`;
                })
                .catch(() => {});
        }

        // ===== Show Toast =====
        function showToast(data) {
            const container = document.getElementById('toastContainer');
            const toast = document.createElement('div');
            const cls = data.passed ? 'pass' : 'fail';
            const icon = data.passed ? 'fa-check-circle' : 'fa-times-circle';
            const label = data.passed ? 'PASS' : 'FAIL';
            toast.className = `toast ${cls}`;
            toast.innerHTML = `
                <i class="fas ${icon} ${cls}"></i>
                <div>
                    <strong>${label}</strong> · ${data.roiName || '全图'}
                    <div style="font-size:11px;color:var(--text-secondary);margin-top:2px">${data.deviceId}</div>
                </div>
            `;
            container.appendChild(toast);

            setTimeout(() => {
                toast.style.animation = 'fadeOut 0.3s ease forwards';
                setTimeout(() => toast.remove(), 300);
            }, 3000);

            // Keep max 5 toasts
            while (container.children.length > 5) {
                container.removeChild(container.firstChild);
            }
        }

        // ===== Send Command =====
        function sendCommand(cmd) {
            const feedback = document.getElementById('cmdFeedback');
            feedback.innerHTML = `<i class="fas fa-spinner fa-spin"></i><span>正在发送指令: ${cmd}...</span>`;

            fetch('/api/command', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ cmd: cmd }),
                })
                .then(r => r.json())
                .then(data => {
                    if (data.ok) {
                        feedback.innerHTML = `<i class="fas fa-check-circle" style="color:var(--accent-green)"></i>
                            <span>指令 <strong>${cmd}</strong> 已发送到边缘设备</span>`;
                    } else {
                        feedback.innerHTML = `<i class="fas fa-exclamation-circle" style="color:var(--accent-red)"></i>
                            <span>发送失败: ${data.error}</span>`;
                    }
                })
                .catch(err => {
                    feedback.innerHTML = `<i class="fas fa-exclamation-circle" style="color:var(--accent-red)"></i>
                        <span>网络错误: ${err.message}</span>`;
                });
        }

        // ===== Simulate Data =====
        function simulateData(count) {
            const feedback = document.getElementById('cmdFeedback');
            feedback.innerHTML = `<i class="fas fa-spinner fa-spin"></i><span>正在生成 ${count} 条模拟数据...</span>`;

            fetch('/api/simulate', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ count: count }),
                })
                .then(r => r.json())
                .then(data => {
                    if (data.error) {
                        feedback.innerHTML = `<i class="fas fa-exclamation-triangle" style="color:var(--accent-yellow)"></i>
                            <span>${data.error}</span>`;
                        return;
                    }
                    feedback.innerHTML = `<i class="fas fa-flask" style="color:var(--accent-purple)"></i>
                        <span>已生成 ${data.simulated} 条模拟数据</span>`;
                })
                .catch(err => {
                    feedback.innerHTML = `<i class="fas fa-exclamation-circle" style="color:var(--accent-red)"></i>
                        <span>模拟失败: ${err.message}</span>`;
                });
        }

        // ===== History Query =====
        let historyPage = 1;
        let historyPageSize = 20;
        let historyTotalPages = 1;

        function queryHistory(page = 1) {
            historyPage = page;
            const device = document.getElementById('historyDevice').value;
            const roi = document.getElementById('historyRoi').value;
            const passed = document.getElementById('historyStatus').value;
            const days = document.getElementById('historyDays').value;

            let url = `/api/results/history?page=${page}&page_size=${historyPageSize}`;
            if (device) url += `&device=${encodeURIComponent(device)}`;
            if (roi) url += `&roi=${encodeURIComponent(roi)}`;
            if (passed) url += `&passed=${passed}`;
            if (days) url += `&days=${days}`;

            fetch(url)
                .then(r => r.json())
                .then(data => {
                    const tbody = document.getElementById('historyBody');
                    const empty = document.getElementById('historyEmpty');
                    const pagination = document.getElementById('historyPagination');

                    if (!data.items || data.items.length === 0) {
                        tbody.innerHTML = '';
                        empty.style.display = 'flex';
                        empty.innerHTML = '<i class="fas fa-inbox"></i><p>未找到匹配的历史数据</p>';
                        pagination.style.display = 'none';
                        document.getElementById('historyTotal').textContent = '0 条';
                        return;
                    }

                    empty.style.display = 'none';
                    pagination.style.display = 'flex';

                    // Update total count
                    document.getElementById('historyTotal').textContent = data.total + ' 条';

                    // Calculate total pages
                    historyTotalPages = Math.ceil(data.total / historyPageSize) || 1;

                    // Render rows
                    tbody.innerHTML = data.items.map(item => {
                        const time = item.dateTime || '--';
                        const badge = item.passed ?
                            '<span class="badge-pass"><i class="fas fa-check"></i> PASS</span>' :
                            '<span class="badge-fail"><i class="fas fa-times"></i> FAIL</span>';
                        const barcodeIcon = item.hasBarcode ? '<i class="fas fa-qrcode" style="color:var(--accent-cyan)"></i>' : '--';

                        return `
                            <tr>
                                <td style="font-size:12px;color:var(--text-secondary)">${time}</td>
                                <td class="device-name" style="font-size:12px">${item.deviceId || '--'}</td>
                                <td>${item.roiName || '全图'}</td>
                                <td>${badge}</td>
                                <td>${item.totalRegionCount ?? '--'}</td>
                                <td>${barcodeIcon}</td>
                            </tr>`;
                    }).join('');

                    // Update pagination
                    updateHistoryPagination();
                })
                .catch(err => {
                    console.error('History query failed:', err);
                });
        }

        function updateHistoryPagination() {
            document.getElementById('historyPageInfo').textContent =
                `第 ${historyPage} 页 / 共 ${historyTotalPages} 页`;
            document.getElementById('historyPrevBtn').disabled = historyPage <= 1;
            document.getElementById('historyNextBtn').disabled = historyPage >= historyTotalPages;
        }

        function historyPrevPage() {
            if (historyPage > 1) queryHistory(historyPage - 1);
        }

        function historyNextPage() {
            if (historyPage < historyTotalPages) queryHistory(historyPage + 1);
        }

        function exportHistory() {
            const device = document.getElementById('historyDevice').value;
            const roi = document.getElementById('historyRoi').value;
            const passed = document.getElementById('historyStatus').value;
            const days = document.getElementById('historyDays').value;

            let url = `/api/results/history?page=1&page_size=10000`;
            if (device) url += `&device=${encodeURIComponent(device)}`;
            if (roi) url += `&roi=${encodeURIComponent(roi)}`;
            if (passed) url += `&passed=${passed}`;
            if (days) url += `&days=${days}`;

            fetch(url)
                .then(r => r.json())
                .then(data => {
                    if (!data.items || data.items.length === 0) {
                        alert('没有可导出的数据');
                        return;
                    }

                    // Build CSV
                    const headers = ['时间', '设备', 'ROI', '结果', '区域数', '条码'];
                    const rows = data.items.map(item => [
                        item.dateTime || '',
                        item.deviceId || '',
                        item.roiName || '',
                        item.passed ? '合格' : '不合格',
                        item.totalRegionCount ?? '',
                        item.hasBarcode ? '有' : '无'
                    ]);

                    let csv = '\uFEFF'; // BOM for Chinese
                    csv += headers.join(',') + '\n';
                    csv += rows.map(r => r.map(c => `"${c}"`).join(',')).join('\n');

                    // Download
                    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
                    const link = document.createElement('a');
                    link.href = URL.createObjectURL(blob);
                    link.download = `检测历史_${new Date().toISOString().slice(0,10)}.csv`;
                    link.click();
                });
        }

        function loadHistoryFilters() {
            // Load devices - clear existing options first (keep first option)
            const deviceSelect = document.getElementById('historyDevice');
            while (deviceSelect.options.length > 1) {
                deviceSelect.remove(1);
            }
            fetch('/api/devices')
                .then(r => r.json())
                .then(devices => {
                    devices.forEach(d => {
                        const opt = document.createElement('option');
                        opt.value = d.device_id;
                        opt.textContent = d.device_id;
                        deviceSelect.appendChild(opt);
                    });
                });

            // Load ROI names - clear existing options first (keep first option)
            const roiSelect = document.getElementById('historyRoi');
            while (roiSelect.options.length > 1) {
                roiSelect.remove(1);
            }
            fetch('/api/stats/by-roi')
                .then(r => r.json())
                .then(roiStats => {
                    Object.keys(roiStats).forEach(name => {
                        const opt = document.createElement('option');
                        opt.value = name;
                        opt.textContent = name;
                        roiSelect.appendChild(opt);
                    });
                });
        }

        // ===== Init =====
        document.addEventListener('DOMContentLoaded', function() {
            initCharts();
            fetchHostInfo();
            fetchStats();
            fetchTodayStats();
            fetchDevices();
            fetchResults();
            loadHistoryFilters();
            connectSSE();

            // Periodic polling as backup
            registerTimer(setInterval(fetchStats, 5000));
            registerTimer(setInterval(fetchTodayStats, 10000));
            registerTimer(setInterval(fetchDevices, 5000));
            registerTimer(setInterval(fetchResults, 8000));
        });

        // Cleanup on page unload
        window.addEventListener('beforeunload', function() {
            clearAllTimers();
            closeEventSource();
        });

        // ===== Host Info (LAN IP) =====
        let hostInfoBound = false;
        function fetchHostInfo() {
            fetch('/api/host-info')
                .then(r => r.json())
                .then(info => {
                    const addr = info.lanIp + ':' + info.port;
                    const el = document.getElementById('lanAddress');
                    const container = document.getElementById('lanInfo');
                    el.textContent = 'http://' + addr;
                    container.style.display = 'flex';

                    // Only bind click handler once
                    if (!hostInfoBound) {
                        hostInfoBound = true;
                        container.onclick = function() {
                            navigator.clipboard.writeText('http://' + addr).then(() => {
                                const old = el.textContent;
                                el.textContent = '已复制!';
                                setTimeout(() => el.textContent = old, 1500);
                            });
                        };
                    }
                });
        }

        // ===== Debug Panel Functions =====
        let debugPanelVisible = false;

        // Trigger mechanism: Ctrl + Left Click (in header area only)
        document.querySelector('.header').addEventListener('mousedown', function(e) {
            if (e.ctrlKey && e.button === 0) {
                e.preventDefault();
                e.stopPropagation();
                openDebugPanel();
            }
        });

        // ESC key to close
        document.addEventListener('keydown', function(e) {
            if (e.key === 'Escape' && debugPanelVisible) {
                closeDebugPanel();
            }
        });

        function openDebugPanel() {
            document.getElementById('debugPanel').style.display = 'block';
            debugPanelVisible = true;
            refreshDebugData();
            addDebugLog('调试面板已打开', 'info');
        }

        function closeDebugPanel() {
            document.getElementById('debugPanel').style.display = 'none';
            debugPanelVisible = false;
            addDebugLog('调试面板已关闭', 'info');
        }

        function addDebugLog(message, type = 'info') {
            const log = document.getElementById('debugLog');
            const time = new Date().toLocaleTimeString('zh-CN', { hour12: false });
            const color = type === 'error' ? 'var(--accent-red)' : 
                         type === 'success' ? 'var(--accent-green)' : 
                         type === 'warning' ? 'var(--accent-yellow)' : 'var(--accent-cyan)';
            const div = document.createElement('div');
            div.style.color = color;
            div.textContent = `[${time}] ${message}`;
            log.insertBefore(div, log.firstChild);
        }

        async function refreshDebugData() {
            try {
                const statsRes = await fetch('/api/stats');
                const s = await statsRes.json();
                console.log(`[DEBUG] refreshDebugData 收到: total=${s.total}, passed=${s.passed}, failed=${s.failed}, passRate=${s.passRate}`);
                document.getElementById('debugTotal').value = s.total;
                document.getElementById('debugPassed').value = s.passed;
                document.getElementById('debugFailed').value = s.failed;
                document.getElementById('debugPassRate').value = s.passRate;

                const roiRes = await fetch('/api/stats/by-roi');
                const roiData = await roiRes.json();
                const roiSelect = document.getElementById('debugRoiSelect');
                roiSelect.innerHTML = '<option value="">选择 ROI</option>';
                for (const [name, data] of Object.entries(roiData)) {
                    const option = document.createElement('option');
                    option.value = name;
                    option.textContent = `${name} (${data.total})`;
                    roiSelect.appendChild(option);
                }

                const deviceRes = await fetch('/api/devices');
                const deviceList = await deviceRes.json();
                const deviceSelect = document.getElementById('debugDeviceSelect');
                deviceSelect.innerHTML = '<option value="">选择设备</option>';
                deviceList.forEach(d => {
                    const option = document.createElement('option');
                    option.value = d.device_id;
                    option.textContent = `${d.device_id} (${d.status})`;
                    deviceSelect.appendChild(option);
                });

                addDebugLog('数据刷新完成', 'success');
            } catch (err) {
                addDebugLog(`数据刷新失败: ${err.message}`, 'error');
            }
        }

        // Auto-update pass rate
        document.getElementById('debugTotal').addEventListener('input', updatePassRate);
        document.getElementById('debugPassed').addEventListener('input', updatePassRate);

        // Auto-fill ROI fields when selecting from dropdown
        document.getElementById('debugRoiSelect').addEventListener('change', async function() {
            const name = this.value;
            if (!name) return;
            try {
                const res = await fetch('/api/stats/by-roi');
                const roiData = await res.json();
                if (roiData[name]) {
                    document.getElementById('debugRoiNewName').value = name;
                    document.getElementById('debugRoiTotal').value = roiData[name].total;
                    document.getElementById('debugRoiPassed').value = roiData[name].passed;
                }
            } catch (e) {}
        });

        // Auto-fill device fields when selecting from dropdown
        document.getElementById('debugDeviceSelect').addEventListener('change', async function() {
            const id = this.value;
            if (!id) return;
            try {
                const res = await fetch('/api/devices');
                const devices = await res.json();
                const d = devices.find(d => d.device_id === id);
                if (d) {
                    document.getElementById('debugDeviceStatus').value = d.status;
                    document.getElementById('debugDeviceCount').value = d.result_count || 0;
                    document.getElementById('debugDeviceUptime').value = Math.floor(d.uptime || 0);
                }
            } catch (e) {}
        });

        function updatePassRate() {
            const total = parseInt(document.getElementById('debugTotal').value) || 0;
            const passed = parseInt(document.getElementById('debugPassed').value) || 0;
            const rate = total > 0 ? (passed / total * 100).toFixed(1) : 0;
            document.getElementById('debugPassRate').value = rate;
        }

        async function updateDebugStats() {
            const total = parseInt(document.getElementById('debugTotal').value) || 0;
            const passed = parseInt(document.getElementById('debugPassed').value) || 0;
            const failed = parseInt(document.getElementById('debugFailed').value) || 0;
            console.log(`[DEBUG] updateDebugStats 发送: total=${total}, passed=${passed}, failed=${failed}`);

            try {
                const res = await fetch('/api/debug/update-stats', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ total, passed, failed })
                });
                const data = await res.json();
                if (data.ok) {
                    addDebugLog('统计数据更新成功', 'success');
                    refreshDebugData();
                    fetchStats();
                } else {
                    addDebugLog(`更新失败: ${data.error}`, 'error');
                }
            } catch (err) {
                addDebugLog(`更新失败: ${err.message}`, 'error');
            }
        }

        async function updateRoi() {
            const oldName = document.getElementById('debugRoiSelect').value;
            const newName = document.getElementById('debugRoiNewName').value;
            const total = parseInt(document.getElementById('debugRoiTotal').value) || 0;
            const passed = parseInt(document.getElementById('debugRoiPassed').value) || 0;

            if (!oldName) {
                addDebugLog('请选择要更新的ROI', 'warning');
                return;
            }

            try {
                const res = await fetch('/api/debug/update-roi', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ oldName, newName, total, passed })
                });
                const data = await res.json();
                if (data.ok) {
                    addDebugLog(`ROI "${oldName}" 更新成功`, 'success');
                    refreshDebugData();
                    fetchStats();
                    fetchResults();
                    loadHistoryFilters();
                } else {
                    addDebugLog(`更新失败: ${data.error}`, 'error');
                }
            } catch (err) {
                addDebugLog(`更新失败: ${err.message}`, 'error');
            }
        }

        async function addNewRoi() {
            const name = document.getElementById('debugRoiNewName').value;
            const total = parseInt(document.getElementById('debugRoiTotal').value) || 0;
            const passed = parseInt(document.getElementById('debugRoiPassed').value) || 0;

            if (!name) {
                addDebugLog('请输入ROI名称', 'warning');
                return;
            }

            try {
                const res = await fetch('/api/debug/add-roi', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name, total, passed })
                });
                const data = await res.json();
                if (data.ok) {
                    addDebugLog(`ROI "${name}" 添加成功`, 'success');
                    refreshDebugData();
                    fetchStats();
                    fetchResults();
                    loadHistoryFilters();
                } else {
                    addDebugLog(`添加失败: ${data.error}`, 'error');
                }
            } catch (err) {
                addDebugLog(`添加失败: ${err.message}`, 'error');
            }
        }

        async function deleteRoi() {
            const name = document.getElementById('debugRoiSelect').value;
            if (!name) {
                addDebugLog('请选择要删除的ROI', 'warning');
                return;
            }
            if (!confirm(`确定要删除ROI "${name}" 吗？`)) {
                addDebugLog('删除操作已取消', 'warning');
                return;
            }

            try {
                const res = await fetch('/api/debug/delete-roi', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name })
                });
                const data = await res.json();
                if (data.ok) {
                    addDebugLog(`ROI "${name}" 已删除`, 'success');
                    refreshDebugData();
                    fetchStats();
                    fetchResults();
                    loadHistoryFilters();
                } else {
                    addDebugLog(`删除失败: ${data.error}`, 'error');
                }
            } catch (err) {
                addDebugLog(`删除失败: ${err.message}`, 'error');
            }
        }

        async function updateDevice() {
            const deviceId = document.getElementById('debugDeviceSelect').value;
            const status = document.getElementById('debugDeviceStatus').value;
            const resultCount = parseInt(document.getElementById('debugDeviceCount').value) || 0;
            const uptime = parseInt(document.getElementById('debugDeviceUptime').value) || 0;

            if (!deviceId) {
                addDebugLog('请选择要更新的设备', 'warning');
                return;
            }

            try {
                const res = await fetch('/api/debug/update-device', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ deviceId, status, resultCount, uptime })
                });
                const data = await res.json();
                if (data.ok) {
                    addDebugLog(`设备 "${deviceId}" 更新成功`, 'success');
                    refreshDebugData();
                    fetchDevices();
                } else {
                    addDebugLog(`更新失败: ${data.error}`, 'error');
                }
            } catch (err) {
                addDebugLog(`更新失败: ${err.message}`, 'error');
            }
        }

        async function addNewDevice() {
            const deviceId = document.getElementById('debugDeviceSelect').value || `device-${Date.now()}`;
            const status = document.getElementById('debugDeviceStatus').value;
            const resultCount = parseInt(document.getElementById('debugDeviceCount').value) || 0;
            const uptime = parseInt(document.getElementById('debugDeviceUptime').value) || 0;

            try {
                const res = await fetch('/api/debug/add-device', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ deviceId, status, resultCount, uptime })
                });
                const data = await res.json();
                if (data.ok) {
                    addDebugLog(`设备 "${deviceId}" 添加成功`, 'success');
                    refreshDebugData();
                    fetchDevices();
                } else {
                    addDebugLog(`添加失败: ${data.error}`, 'error');
                }
            } catch (err) {
                addDebugLog(`添加失败: ${err.message}`, 'error');
            }
        }

        async function generateDemoData() {
            try {
                const res = await fetch('/api/debug/generate-demo', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                });
                const data = await res.json();
                if (data.ok) {
                    addDebugLog(`已生成 ${data.count} 条演示数据`, 'success');
                    refreshDebugData();
                    fetchStats();
                    fetchDevices();
                    fetchResults();
                } else {
                    addDebugLog(`生成失败: ${data.error}`, 'error');
                }
            } catch (err) {
                addDebugLog(`生成失败: ${err.message}`, 'error');
            }
        }

        async function clearAllData() {
            if (!confirm('确定要清空所有数据吗？此操作不可恢复！')) {
                addDebugLog('清空操作已取消', 'warning');
                return;
            }

            try {
                const res = await fetch('/api/debug/clear-data', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                });
                const data = await res.json();
                if (data.ok) {
                    addDebugLog('所有数据已清空', 'success');
                    refreshDebugData();
                    fetchStats();
                    fetchDevices();
                    fetchResults();
                } else {
                    addDebugLog(`清空失败: ${data.error}`, 'error');
                }
            } catch (err) {
                addDebugLog(`清空失败: ${err.message}`, 'error');
            }
        }

        async function exportDebugData() {
            try {
                const res = await fetch('/api/debug/export');
                const data = await res.json();
                const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `debug-data-${new Date().toISOString().slice(0,10)}.json`;
                a.click();
                addDebugLog('调试数据已导出', 'success');
            } catch (err) {
                addDebugLog(`导出失败: ${err.message}`, 'error');
            }
        }
