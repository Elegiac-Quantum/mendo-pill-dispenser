let schedules = [];
let editingId = null;
let uiLanguage = "en";

const rows = document.querySelector("#schedule-rows");
const dialog = document.querySelector("#schedule-dialog");
const form = document.querySelector("#schedule-form");
const toast = document.querySelector("#toast");
const saveButton = form.querySelector("button[type=submit]");
const audioConfirm = document.querySelector("#audio-confirm");
const playAudio = document.querySelector("#play-audio");
const stopAudio = document.querySelector("#stop-audio");
const audioState = document.querySelector("#audio-state");
const testMicrophone = document.querySelector("#test-microphone");
const spokenFile = document.querySelector("#spoken-file");
const spokenState = document.querySelector("#spoken-state");
const spokenSoundMode = document.querySelector("#spoken-sound-mode");
const alarmFile = document.querySelector("#alarm-file");
const alarmState = document.querySelector("#alarm-state");
const alarmSoundMode = document.querySelector("#alarm-sound-mode");
const aiConfigForm = document.querySelector("#ai-config-form");
const aiConfigState = document.querySelector("#ai-config-state");
const talkToAi = document.querySelector("#talk-to-ai");
const aiTalkState = document.querySelector("#ai-talk-state");
const aiVolume = document.querySelector("#ai-volume");
const aiVolumeValue = document.querySelector("#ai-volume-value");
const saveAiVolume = document.querySelector("#save-ai-volume");
const aiVad = document.querySelector("#ai-vad");
const aiVadValue = document.querySelector("#ai-vad-value");
const saveAiVad = document.querySelector("#save-ai-vad");
const resetAiVad = document.querySelector("#reset-ai-vad");
const aiVadRange = document.querySelector("#ai-vad-range");
const syncClock = document.querySelector("#sync-clock");
const languageToggle = document.querySelector("#language-toggle");
const medicineTaken = document.querySelector("#medicine-taken");
const medicineSnooze = document.querySelector("#medicine-snooze");
const reminderState = document.querySelector("#reminder-state");
const reminderHistory = document.querySelector("#reminder-history");
const nextMedication = document.querySelector("#next-medication");
const nextDoseInstruction = document.querySelector("#next-dose-instruction");
const nextDoseTime = document.querySelector("#next-dose-time");
const activityList = document.querySelector("#activity-list");
const movementConfirm = document.querySelector("#movement-confirm");
const testDispenser = document.querySelector("#test-dispenser");
const dispenserTestState = document.querySelector("#dispenser-test-state");
const referenceConfirm = document.querySelector("#reference-confirm");
const setTrayReference = document.querySelector("#set-tray-reference");
const trayReferenceState = document.querySelector("#tray-reference-state");
const patientButtonState = document.querySelector("#patient-button-state");
const patientButtonCount = document.querySelector("#patient-button-count");
const originalText = new WeakMap();

const zhText = {
  "Smart Pill Dispenser": "智能药盒",
  "Setup mode": "设置模式",
  "Overview": "概览",
  "Schedules": "服药计划",
  "Device": "设备",
  "Settings": "设置",
  "Test": "测试",
  "Patient button test": "实体服药按钮测试",
  "Move one slot": "向前移动一格",
  "Press the physical “I took the medicine” button. Testing without an active reminder does not record a dose.": "按下实体“我已服药”按钮；没有正在提醒时，测试不会记录服药。",
  "Waiting for button...": "等待按下按钮……",
  "Detected presses: 0": "检测到按压：0 次",
  "Need help?": "需要帮助？",
  "Check the user guide": "查看使用指南",
  "Checking RTC": "正在检查时钟",
  "Time not confirmed": "时间未确认",
  "Sync time": "同步时间",
  "Next dose": "下次服药",
  "No active schedule": "暂无启用计划",
  "Caregiver setup required": "需要照护者设置",
  "No dose scheduled": "暂无出药计划",
  "Automatic dispensing runs only for a validated RTC and an enabled schedule.": "仅在时钟有效且计划已启用时自动出药。",
  "Automatic movement stays locked until the RTC and a saved schedule are validated.": "时钟和服药计划验证前，自动机械动作保持锁定。",
  "Device health": "设备状态",
  "I2C bus": "I2C 总线",
  "I/O expander": "I/O 扩展器",
  "TF card": "TF 卡",
  "Audio codec": "音频芯片",
  "Rotary drive": "旋转驱动",
  "RTC hardware": "时钟模块",
  "Medication schedules": "服药计划",
  "Enable one for RTC reminders. Automatic dispensing remains locked.": "可启用一个到点提醒；自动出药仍保持锁定。",
  "Add schedule": "添加计划",
  "Medicine": "药品",
  "Dose": "剂量",
  "Times": "时间",
  "Status": "状态",
  "Actions": "操作",
  "Checking": "检查中",
  "Current medication reminder": "当前服药提醒",
  "No medication reminder is active.": "当前没有服药提醒。",
  "I took the medicine": "我已服药",
  "Remind me in 10 minutes": "10 分钟后提醒",
  "No response recorded yet.": "尚无服药操作记录。",
  "AI voice connection": "AI 语音连接",
  "Connect this dispenser to the internet and your private family-voice server. The setup hotspot remains available for recovery.": "将药盒连接到互联网和家庭语音服务；设置热点会保留用于恢复。",
  "Wi-Fi name": "Wi-Fi 名称",
  "Wi-Fi password": "Wi-Fi 密码",
  "Secure voice server": "安全语音服务器",
  "Device secret": "设备密钥",
  "Save and connect": "保存并连接",
  "Checking configuration": "正在检查配置",
  "Start talking": "开始对话",
  "AI idle": "AI 空闲",
  "AI voice volume:": "AI 语音音量：",
  "Save volume": "保存音量",
  "Above 85% may be loud at close range. Increase gradually.": "音量超过 85% 时近距离可能较响，请逐步调高。",
  "Silence threshold:": "静音阈值：",
  "Save silence threshold": "保存静音阈值",
  "Last detected range: unavailable": "最近检测范围：暂无",
  "Recent activity": "最近活动",
  "Download dose history (CSV)": "下载服药记录（CSV）",
  "The history is stored on the TF card.": "服药记录保存在 TF 卡中。",
  "Setup hotspot password": "设置热点密码",
  "Loading hotspot name...": "正在读取热点名称……",
  "New hotspot password": "新热点密码",
  "Confirm new password": "再次输入新密码",
  "Change hotspot password": "修改热点密码",
  "The dispenser will restart. Reconnect using the new password.": "药盒将重新启动，请使用新密码重新连接。",
  "Optional caregiver PIN": "可选管理员 PIN",
  "Checking PIN protection...": "正在检查 PIN 保护……",
  "Caregiver PIN (4–8 digits)": "管理员 PIN（4–8 位数字）",
  "Unlock settings": "解锁设置",
  "Set or change PIN": "设置或修改 PIN",
  "Disable PIN": "关闭 PIN",
  "PIN protection is optional. Patient reminder buttons and AI conversation remain available.": "PIN 保护为可选功能；患者服药按钮和 AI 对话不受影响。",
  "Now": "现在",
  "Setup mode active; automatic dispensing locked": "设置模式已启用；自动出药保持锁定",
  "Low-volume speaker test": "低音量扬声器测试",
  "Plays one quiet 0.5-second tone. It does not move the dispenser or activate a schedule.": "播放一次安静的 0.5 秒测试音，不会移动机构或启用计划。",
  "I am ready for a quiet test tone.": "我已准备好播放安静的测试音。",
  "Play 0.5 second tone": "播放 0.5 秒测试音",
  "Stop sound": "停止声音",
  "Test microphone (3 seconds)": "测试麦克风（3 秒）",
  "Audio idle": "音频空闲",
  "Offline voice preview": "离线语音预览",
  "Choose a 16 kHz, mono, 16-bit PCM WAV file up to 30 seconds. It is validated before replacing the previous preview.": "请选择最长 30 秒的 16 kHz、单声道、16 位 PCM WAV 文件；验证通过后才会替换原语音。",
  "Upload validated WAV": "上传已验证 WAV",
  "Play saved preview": "播放已保存语音",
  "No upload attempted": "尚未上传",
  "Test dispenser": "测试出药器",
  "Tray alignment reference": "药盘对准基准",
  "Manually align one compartment with the outlet, then save that position as the movement reference. The tray will not move.": "手动将一个药格对准出药口，再把当前位置保存为运动基准；药盘不会转动。",
  "The compartment is correctly aligned with the outlet.": "药格已经正确对准出药口。",
  "Use current position as start": "将当前位置设为起始位置",
  "Alignment reference unchanged.": "尚未更改药盘基准。",
  "(manual test only)": "（仅人工测试）",
  "I have removed medicine and kept hands clear.": "我已移除药物，并让双手远离机构。",
  "Automatic dispensing remains locked.": "自动出药仍保持锁定。",
  "Add schedule draft": "添加服药计划",
  "Medication name": "药品名称",
  "Dose instruction": "剂量说明",
  "Daily time": "每日时间",
  "Cancel": "取消",
  "Save inactive draft": "保存未启用计划"
};

zhText["Enable a schedule for automatic dispensing and reminders. Load medicine in chronological order."] =
  "启用计划后将自动出药并提醒。请按服药时间顺序装入药物。";
zhText["Load medicine in chronological order. Use one schedule per dispensing time and list medicines together."] =
  "请按服药时间顺序装药；同一服药时间只建一个计划，多种药请写在一起。";
zhText["Moves the dispenser forward by one slot. It does not mark medicine as taken."] =
  "将药盘向前移动一格，但不会记录为已服药。";
zhText["Manual test ready."] = "可进行手动测试。";
zhText["No recent medication activity"] = "暂无最近服药记录";
zhText["Medication reminder sound"] = "服药提醒声音";
zhText["Choose the built-in default sound or upload a custom audio file up to 30 seconds. The browser converts it for the dispenser."] =
  "可选择内置默认声音，或上传最长 30 秒的自定义音频；网页会自动转换为药盒所需格式。";
zhText["Reminder sound"] = "提醒声音";
zhText["Built-in default sound"] = "内置默认声音";
zhText["Uploaded custom sound"] = "已上传的自定义声音";
zhText["Upload custom WAV"] = "上传自定义 WAV";
zhText["Preview custom sound"] = "试听自定义声音";
zhText["Save reminder sound"] = "保存提醒声音";
zhText["Built-in default sound selected"] = "已选择内置默认声音";
zhText["Speaker volume (AI and reminders):"] = "扬声器音量（AI 对话与服药提醒）：";

zhText["After the spoken medication reminder, play the built-in alarm bell or an uploaded audio file once."] =
  "\u64ad\u653e\u201c\u8bf7\u5403\u836f\u201d\u8bed\u97f3\u540e\uff0c\u518d\u64ad\u653e\u4e00\u6b21\u5185\u7f6e\u95f9\u949f\u94c3\u58f0\u6216\u4e0a\u4f20\u7684\u97f3\u9891\u3002";
zhText["Alarm sound"] = "\u95f9\u949f\u94c3\u58f0";
zhText["Built-in alarm bell"] = "\u5185\u7f6e\u95f9\u949f\u94c3\u58f0";
zhText["Uploaded alarm sound"] = "\u5df2\u4e0a\u4f20\u7684\u95f9\u949f\u94c3\u58f0";
zhText["Customize the spoken message and the alarm separately. MP3, M4A, AAC, OGG and WAV files up to 30 seconds are converted automatically."] =
  "\u64ad\u62a5\u548c\u94c3\u58f0\u53ef\u5206\u522b\u8bbe\u7f6e\u3002\u652f\u6301\u6700\u957f 30 \u79d2\u7684 MP3\u3001M4A\u3001AAC\u3001OGG \u548c WAV\uff0c\u7f51\u9875\u4f1a\u81ea\u52a8\u8f6c\u6362\u3002";
zhText["Spoken medication message"] = "\u5403\u836f\u8bed\u97f3\u64ad\u62a5";
zhText["Spoken message"] = "\u8bed\u97f3\u64ad\u62a5";
zhText["Built-in \u201cPlease take your medicine\u201d"] = "\u5185\u7f6e\u201c\u8bf7\u5403\u836f\u201d";
zhText["Uploaded spoken message"] = "\u5df2\u4e0a\u4f20\u7684\u8bed\u97f3\u64ad\u62a5";
zhText["Upload spoken message"] = "\u4e0a\u4f20\u8bed\u97f3\u64ad\u62a5";
zhText["Preview spoken message"] = "\u8bd5\u542c\u8bed\u97f3\u64ad\u62a5";
zhText["Save spoken message"] = "\u4fdd\u5b58\u8bed\u97f3\u64ad\u62a5";
zhText["Built-in spoken message selected"] = "\u5df2\u9009\u62e9\u5185\u7f6e\u8bed\u97f3\u64ad\u62a5";
zhText["Alarm after the message"] = "\u64ad\u62a5\u540e\u7684\u95f9\u949f\u94c3\u58f0";
zhText["Upload alarm sound"] = "\u4e0a\u4f20\u95f9\u949f\u94c3\u58f0";
zhText["Preview alarm sound"] = "\u8bd5\u542c\u95f9\u949f\u94c3\u58f0";
zhText["Save alarm sound"] = "\u4fdd\u5b58\u95f9\u949f\u94c3\u58f0";
zhText["Built-in alarm bell selected"] = "\u5df2\u9009\u62e9\u5185\u7f6e\u95f9\u949f\u94c3\u58f0";
zhText["Xiaozhi AI is already built in and bound. Only update Wi-Fi here; the setup hotspot remains available for recovery."] =
  "\u5c0f\u667a AI \u5df2\u5185\u7f6e\u5e76\u7ed1\u5b9a\u3002\u6b64\u5904\u53ea\u9700\u66f4\u65b0 Wi-Fi\uff0c\u8bbe\u7f6e\u70ed\u70b9\u4ecd\u4fdd\u7559\u7528\u4e8e\u6062\u590d\u3002";
zhText["(recommended/default: 80)"] = "\uff08\u63a8\u8350/\u9ed8\u8ba4\uff1a80\uff09";
zhText["Restore recommended 80"] = "\u6062\u590d\u63a8\u8350\u503c 80";
zhText["80 is the value previously verified successfully on this dispenser."] =
  "80 \u662f\u8fd9\u53f0\u836f\u76d2\u4e4b\u524d\u5b9e\u6d4b\u6210\u529f\u7684\u6570\u503c\u3002";

const t = (english, chinese) => uiLanguage === "zh" ? chinese : english;

function applyLanguage() {
  document.documentElement.lang = uiLanguage === "zh" ? "zh-CN" : "en";
  languageToggle.textContent = uiLanguage === "zh" ? "English" : "中文";
  const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
  let node;
  while ((node = walker.nextNode())) {
    if (node.parentElement?.id === "language-toggle" ||
        node.parentElement?.tagName === "SCRIPT") continue;
    if (!originalText.has(node)) originalText.set(node, node.nodeValue);
    const source = originalText.get(node);
    const english = source.trim();
    const translated = uiLanguage === "zh" && zhText[english]
      ? zhText[english] : english;
    node.nodeValue = source.replace(english, translated);
  }
  renderSchedules();
}

fetch("/api/ui/language", { cache: "no-store" }).then((response) => response.json())
  .then((result) => { uiLanguage = result.language; applyLanguage(); }).catch(() => {});

languageToggle.addEventListener("click", async () => {
  const next = uiLanguage === "zh" ? "en" : "zh";
  const response = await fetch("/api/ui/language", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ language: next })
  });
  if (!response.ok) return showToast(t("Language save failed", "语言保存失败"));
  uiLanguage = next;
  applyLanguage();
});

async function refreshReminder() {
  const status = await fetch("/api/reminders/status", { cache: "no-store" })
    .then((response) => response.json()).catch(() => null);
  if (!status) return;
  if (status.activation_code) {
    aiTalkState.textContent = t(
      `Bind this dispenser in Xiaozhi using code ${status.activation_code}`,
      `请在小智控制台使用绑定码 ${status.activation_code}`);
    talkToAi.disabled = false;
    return;
  }
  reminderState.textContent = status.due
    ? (uiLanguage === "zh"
        ? `该服用：${status.medicine}`
        : `Medication due: ${status.medicine}`)
    : (uiLanguage === "zh"
        ? "当前没有服药提醒。"
        : "No medication reminder is active.");
  medicineTaken.disabled = !status.due;
  medicineSnooze.disabled = !status.due;
  const latestEvent = status.last_event === "taken"
    ? t("Medicine marked as taken", "已确认服药")
    : status.last_event === "dispense_fault"
      ? t("Dispensing fault; caregiver check required", "出药失败，需要护理人员检查")
      : t("Reminder snoozed", "已设置稍后提醒");
  reminderHistory.textContent = status.last_event
    ? (uiLanguage === "zh"
        ? `最近记录：${status.last_time} · ${latestEvent}`
        : `Latest: ${status.last_time} · ${latestEvent}`)
    : (uiLanguage === "zh" ? "尚无服药操作记录。" : "No response recorded yet.");
  activityList.innerHTML = status.last_event
    ? `<li><time>${escapeText(status.last_time)}</time><span>${latestEvent}</span></li>`
    : `<li><time>${t("Now", "现在")}</time><span>${t("No recent medication activity", "暂无最近服药记录")}</span></li>`;
}

async function sendReminderAction(action) {
  medicineTaken.disabled = true;
  medicineSnooze.disabled = true;
  const response = await fetch("/api/reminders/status", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ action })
  });
  if (!response.ok) showToast(uiLanguage === "zh" ? "当前没有可处理的提醒" : "No active reminder");
  window.setTimeout(refreshReminder, 500);
}

medicineTaken.addEventListener("click", () => sendReminderAction("taken"));
medicineSnooze.addEventListener("click", () => sendReminderAction("snooze"));
window.setInterval(refreshReminder, 1000);
refreshReminder();

async function refreshPatientButton() {
  const status = await fetch("/api/button/status", { cache: "no-store" })
    .then((response) => response.json()).catch(() => null);
  if (!status || !patientButtonState) return;
  patientButtonState.textContent = status.pressed
    ? t("Button pressed", "按钮已按下")
    : t("Button released", "按钮已松开");
  patientButtonState.className =
    `button-indicator ${status.pressed ? "pressed" : ""}`;
  patientButtonCount.textContent = uiLanguage === "zh"
    ? `检测到按压：${status.press_count} 次`
    : `Detected presses: ${status.press_count}`;
}
window.setInterval(refreshPatientButton, 250);
refreshPatientButton();


function escapeText(value) {
  const span = document.createElement("span");
  span.textContent = value;
  return span.innerHTML;
}

function formatTime(minutes) {
  return `${String(Math.floor(minutes / 60)).padStart(2, "0")}:${String(minutes % 60).padStart(2, "0")}`;
}

function renderSchedules() {
  rows.innerHTML = schedules.map((schedule) => `
    <tr>
      <td>${escapeText(schedule.medication)}<br>
          <button class="edit" data-${schedule.active ? "deactivate" : "activate"}="${escapeText(schedule.id)}">${schedule.active ? (uiLanguage === "zh" ? "停用提醒" : "Disable reminder") : (uiLanguage === "zh" ? "启用提醒" : "Enable reminder")}</button></td>
      <td>${escapeText(schedule.dose_instruction)}</td>
      <td>${schedule.times.map(formatTime).join(", ")}</td>
      <td><strong class="${schedule.active ? "healthy" : "pending"}">${schedule.active ? (uiLanguage === "zh" ? "提醒已启用" : "Reminder enabled") : (uiLanguage === "zh" ? "已保存，未启用" : "Saved draft - not active")}</strong></td>
      <td><button class="edit" data-edit="${escapeText(schedule.id)}">${uiLanguage === "zh" ? "编辑" : "Edit"}</button>
          <button class="edit" data-delete="${escapeText(schedule.id)}">${uiLanguage === "zh" ? "删除" : "Delete"}</button></td>
    </tr>`).join("");
  renderNextDose();
}

function renderNextDose() {
  const now = new Date();
  const currentMinute = now.getHours() * 60 + now.getMinutes();
  let next = null;
  schedules.filter((schedule) => schedule.active).forEach((schedule) => {
    schedule.times.forEach((minute) => {
      const minutesAway = minute > currentMinute
        ? minute - currentMinute
        : 24 * 60 - currentMinute + minute;
      if (!next || minutesAway < next.minutesAway) {
        next = { schedule, minute, minutesAway };
      }
    });
  });
  if (!next) {
    nextMedication.textContent = t("No active schedule", "暂无启用计划");
    nextDoseInstruction.textContent = t("Caregiver setup required", "需要照护者设置");
    nextDoseTime.textContent = t("No dose scheduled", "暂无出药计划");
    return;
  }
  nextMedication.textContent = next.schedule.medication;
  nextDoseInstruction.textContent = next.schedule.dose_instruction;
  const day = next.minute <= currentMinute
    ? t("Tomorrow", "明天") : t("Today", "今天");
  nextDoseTime.textContent =
    `${day} ${formatTime(next.minute)} · ${t("Reminder enabled", "提醒已启用")}`;
}

function showToast(message) {
  toast.textContent = message;
  toast.classList.add("show");
  window.setTimeout(() => toast.classList.remove("show"), 2600);
}

async function loadSchedules() {
  const response = await fetch("/api/schedules", { cache: "no-store" });
  if (!response.ok) throw new Error("Schedule storage unavailable");
  const payload = await response.json();
  schedules = payload.drafts || [];
  renderSchedules();
  if (payload.storage !== "ready") throw new Error("Schedule storage fault");
}

document.querySelector("#add-schedule").addEventListener("click", () => {
  editingId = null;
  form.reset();
  form.querySelector("h2").textContent = t("Add schedule draft", "添加服药计划");
  dialog.showModal();
});

rows.addEventListener("click", async (event) => {
  const editId = event.target.dataset.edit;
  const deleteId = event.target.dataset.delete;
  const activateId = event.target.dataset.activate;
  const deactivateId = event.target.dataset.deactivate;
  if (editId) {
    const schedule = schedules.find((item) => item.id === editId);
    editingId = editId;
    form.elements.medicine.value = schedule.medication;
    form.elements.dose.value = schedule.dose_instruction;
    form.elements.time.value = formatTime(schedule.times[0]);
    form.querySelector("h2").textContent = t("Edit saved draft", "编辑服药计划");
    dialog.showModal();
  } else if (activateId && window.confirm(t(
    "Enable automatic dispensing and reminders for this schedule? Confirm that medicine is loaded in chronological order.",
    "要为此计划启用自动出药和提醒吗？请确认药物已按服药时间顺序装入。"
  ))) {
    const response = await fetch("/api/reminders/active", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id: activateId, confirmed: true })
    });
    if (!response.ok) return showToast(t(
      "Enable failed; check RTC, servo, or duplicate dispensing time",
      "启用失败：请检查时钟、舵机或是否存在重复出药时间"
    ));
    const payload = await response.json();
    schedules = payload.drafts;
    renderSchedules();
    showToast(t("Automatic dispensing and reminders enabled",
                "自动出药和提醒已启用"));
  } else if (deactivateId && window.confirm(t(
    "Disable this schedule reminder?", "确定停用此服药提醒吗？"))) {
    const response = await fetch("/api/reminders/active", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id: deactivateId, enabled: false })
    });
    if (!response.ok) return showToast(t("Disable failed", "停用失败"));
    const payload = await response.json();
    schedules = payload.drafts;
    renderSchedules();
    showToast(t("Reminder disabled", "提醒已停用"));
  } else if (deleteId && window.confirm(t(
    "Delete this schedule draft?", "确定删除此服药计划吗？"))) {
    const response = await fetch(`/api/schedules/${encodeURIComponent(deleteId)}`, { method: "DELETE" });
    if (!response.ok) return showToast(t(
      "Delete failed; previous draft was kept", "删除失败，原计划已保留"));
    const payload = await response.json();
    schedules = payload.drafts;
    renderSchedules();
    showToast(t("Saved draft deleted", "服药计划已删除"));
  }
});

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (!form.reportValidity()) return;
  const data = new FormData(form);
  const [hour, minute] = String(data.get("time")).split(":").map(Number);
  const body = JSON.stringify({
    medication: data.get("medicine"),
    dose_instruction: data.get("dose"),
    times: [hour * 60 + minute]
  });
  saveButton.disabled = true;
  try {
    const response = await fetch(editingId ? `/api/schedules/${encodeURIComponent(editingId)}` : "/api/schedules", {
      method: editingId ? "PUT" : "POST",
      headers: { "Content-Type": "application/json" },
      body
    });
    if (!response.ok) throw new Error("Save rejected");
    const payload = await response.json();
    schedules = payload.drafts;
    renderSchedules();
    dialog.close();
    showToast(t("Saved to device as an inactive draft",
                "已保存到药盒，当前尚未启用"));
  } catch (error) {
    showToast(t("Save failed; previous data was kept",
                "保存失败，原数据已保留"));
  } finally {
    saveButton.disabled = false;
  }
});

document.querySelectorAll(".nav-item").forEach((button) => {
  button.addEventListener("click", () => {
    document.querySelectorAll(".nav-item").forEach((item) => item.classList.toggle("active", item === button));
    const targets = {
      overview: "overview-view",
      schedules: "schedules-section",
      device: "device-section",
      settings: "settings-section",
      test: "test-section"
    };
    document.querySelector(`#${targets[button.dataset.view]}`)
      ?.scrollIntoView({ behavior: "smooth", block: "start" });
  });
});

loadSchedules().catch(() => showToast(
  t("Schedule storage unavailable", "无法读取服药计划")));

syncClock.addEventListener("click", async () => {
  const now = new Date();
  syncClock.disabled = true;
  try {
    const response = await fetch("/api/clock", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        year: now.getFullYear(), month: now.getMonth() + 1, day: now.getDate(),
        hour: now.getHours(), minute: now.getMinutes(), second: now.getSeconds(),
        weekday: now.getDay()
      })
    });
    if (!response.ok) throw new Error();
    document.querySelector("#clock-state").textContent =
      t("Time synchronized", "时间已同步");
    document.querySelector("#device-time").textContent = now.toLocaleString();
    showToast(t("Time saved; device is restarting",
                "时间已保存，药盒正在重启"));
  } catch (_) {
    showToast(t("Time synchronization failed", "时间同步失败"));
    syncClock.disabled = false;
  }
});

async function loadAiConfigStatus() {
  const response = await fetch("/api/ai/config", { cache: "no-store" });
  if (!response.ok) throw new Error();
  const status = await response.json();
  aiConfigState.textContent = status.sta_connected
    ? t("Internet connected; Xiaozhi AI ready",
        "互联网已连接，AI 服务配置已保存")
    : status.sta_connecting
      ? t("Configuration saved; connecting to Wi-Fi",
          "配置已保存，正在连接 Wi-Fi")
      : status.wifi_configured
        ? t("Configuration saved; Wi-Fi currently offline",
            "配置已保存，Wi-Fi 当前离线")
        : t("Not configured", "尚未配置");
}

aiConfigForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (!aiConfigForm.reportValidity()) return;
  const submit = aiConfigForm.querySelector("button[type=submit]");
  const values = Object.fromEntries(new FormData(aiConfigForm));
  submit.disabled = true;
  aiConfigState.textContent = t("Saving safely...", "正在安全保存……");
  try {
    const response = await fetch("/api/ai/config", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(values),
    });
    if (!response.ok) throw new Error();
    aiConfigForm.reset();
    aiConfigState.textContent = t(
      "Saved; connecting without disabling setup hotspot",
      "已保存；正在连接，设置热点会继续保留");
    window.setTimeout(() => loadAiConfigStatus().catch(() => {}), 3000);
  } catch (_) {
    aiConfigState.textContent = t(
      "Save failed; previous configuration was kept",
      "保存失败，原配置已保留");
  } finally {
    submit.disabled = false;
  }
});

loadAiConfigStatus().catch(() => {
  aiConfigState.textContent =
    t("Configuration status unavailable", "无法读取配置状态");
});

let volumeSaveTimer;

async function persistSpeakerVolume() {
  saveAiVolume.disabled = true;
  try {
    const response = await fetch("/api/ai/volume", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ volume: Number(aiVolume.value) }),
    });
    if (!response.ok) throw new Error();
    const saved = await response.json();
    aiVolume.value = saved.volume;
    aiVolumeValue.textContent = `${saved.volume}%`;
    aiTalkState.textContent = t(
      `Speaker volume saved at ${saved.volume}%`,
      `扬声器音量已保存为 ${saved.volume}%`);
    return true;
  } catch {
    aiTalkState.textContent = t("Volume save failed", "音量保存失败");
    return false;
  } finally {
    saveAiVolume.disabled = false;
  }
}

aiVolume.addEventListener("input", () => {
  aiVolumeValue.textContent = `${aiVolume.value}%`;
  window.clearTimeout(volumeSaveTimer);
  volumeSaveTimer = window.setTimeout(persistSpeakerVolume, 600);
});

fetch("/api/ai/volume", { cache: "no-store" }).then((response) => response.json())
  .then((result) => {
    aiVolume.value = result.volume;
    aiVolumeValue.textContent = `${result.volume}%`;
  }).catch(() => {});

saveAiVolume.addEventListener("click", async () => {
  window.clearTimeout(volumeSaveTimer);
  await persistSpeakerVolume();
});

aiVad.addEventListener("input", () => { aiVadValue.textContent = aiVad.value; });

async function loadVad() {
  const result = await fetch("/api/ai/vad", { cache: "no-store" })
    .then((response) => response.json());
  aiVad.value = result.threshold;
  aiVadValue.textContent = result.threshold;
  aiVadRange.textContent = result.minimum
    ? t(`Last detected range: ${result.minimum} to ${result.maximum}`,
        `最近检测范围：${result.minimum} 至 ${result.maximum}`)
    : t("Last detected range: unavailable", "最近检测范围：暂无");
}

loadVad().catch(() => {});

saveAiVad.addEventListener("click", async () => {
  saveAiVad.disabled = true;
  try {
    const response = await fetch("/api/ai/vad", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ threshold: Number(aiVad.value) }),
    });
    if (!response.ok) throw new Error();
    await loadVad();
    aiTalkState.textContent = t(
      `Silence threshold saved at ${aiVad.value}`,
      `静音阈值已保存为 ${aiVad.value}`);
  } catch (_) {
    aiTalkState.textContent =
      t("Silence threshold save failed", "静音阈值保存失败");
  } finally {
    saveAiVad.disabled = false;
  }
});

resetAiVad.addEventListener("click", () => {
  aiVad.value = 80;
  aiVadValue.textContent = "80";
  saveAiVad.click();
});

async function refreshAiState() {
  const status = await fetch("/api/ai/conversation", { cache: "no-store" })
    .then((result) => result.json()).catch(() => null);
  if (!status) return;
  aiTalkState.textContent = {
    connecting: t("Connecting securely...", "正在安全连接…"),
    listening: t("Listening - pauses up to 3 seconds are okay", "正在聆听，可停顿 3 秒"),
    thinking: t("Thinking...", "正在思考…"),
    speaking: t("Speaking", "正在回答"),
    failed: t(`Conversation failed (${status.last_error || "unknown"}); medicine functions are unaffected`,
              `对话失败（${status.last_error || "未知错误"}）；服药功能不受影响`),
    idle: t("AI idle · wake ready", "AI 空闲 · 可语音唤醒"),
  }[status.state] || status.state;
  talkToAi.disabled = status.state !== "idle";
}

window.setInterval(refreshAiState, 500);
refreshAiState();

talkToAi.addEventListener("click", async () => {
  talkToAi.disabled = true;
  try {
    const response = await fetch("/api/ai/conversation", { method: "POST" });
    if (!response.ok) throw new Error();
    await refreshAiState();
  } catch (_) {
    aiTalkState.textContent = t(
      "AI unavailable; medicine functions are unaffected",
      "AI 暂不可用，服药功能不受影响");
    talkToAi.disabled = false;
  }
});

audioConfirm.addEventListener("change", () => { playAudio.disabled = !audioConfirm.checked; });

testMicrophone.addEventListener("click", async () => {
  testMicrophone.disabled = true;
  audioState.textContent = t(
    "Listening for 3 seconds; recording is not saved",
    "正在监听 3 秒，录音不会保存");
  try {
    const status = await fetch("/api/audio/status", { cache: "no-store" }).then((response) => response.json());
    const response = await fetch("/api/audio/microphone-level", {
      method: "POST",
      headers: { "X-Audio-Nonce": status.nonce },
    });
    const result = response.ok ? await response.json() : null;
    audioState.textContent = result
      ? t(`Microphone peaks — left: ${result.peak_left}, right: ${result.peak_right} (not saved)`,
          `麦克风峰值——左：${result.peak_left}，右：${result.peak_right}（未保存）`)
      : t("Microphone test failed", "麦克风测试失败");
  } catch (_) {
    audioState.textContent = t("Microphone test failed", "麦克风测试失败");
  } finally {
    testMicrophone.disabled = false;
  }
});

playAudio.addEventListener("click", async () => {
  playAudio.disabled = true;
  try {
    const status = await fetch("/api/audio/status", { cache: "no-store" }).then((response) => response.json());
    const response = await fetch("/api/audio/test-tone", {
      method: "POST",
      headers: { "X-Audio-Nonce": status.nonce }
    });
    if (!response.ok) throw new Error();
    audioState.textContent =
      t("Playing quiet test tone", "正在播放低音量测试音");
    window.setTimeout(async () => {
      const diagnostic = await fetch("/api/audio/status", { cache: "no-store" })
        .then((result) => result.json()).catch(() => null);
      audioState.textContent = diagnostic
        ? t(`Diagnostic: ${diagnostic.writes} writes, ${diagnostic.frames} frames, code ${diagnostic.write_result}; regs ${diagnostic.registers.join("-")}`,
            `诊断：写入 ${diagnostic.writes} 次，${diagnostic.frames} 帧，代码 ${diagnostic.write_result}；寄存器 ${diagnostic.registers.join("-")}`)
        : t("Audio diagnostic unavailable", "无法读取音频诊断");
    }, 700);
  } catch (_) {
    audioState.textContent =
      t("Audio test unavailable", "音频测试不可用");
  } finally {
    audioConfirm.checked = false;
  }
});

stopAudio.addEventListener("click", async () => {
  await fetch("/api/audio/stop", { method: "POST" }).catch(() => {});
  audioState.textContent = t("Stop requested", "已请求停止声音");
  audioConfirm.checked = false;
  playAudio.disabled = true;
});

function soundControls(target) {
  return target === "spoken"
    ? { mode: spokenSoundMode, file: spokenFile, state: spokenState }
    : { mode: alarmSoundMode, file: alarmFile, state: alarmState };
}

async function setReminderSound(target, mode) {
  const response = await fetch("/api/audio/reminder-sound", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ target, mode })
  }).catch(() => null);
  if (!response?.ok) return false;
  const controls = soundControls(target);
  controls.mode.value = mode;
  controls.state.textContent = mode === "custom"
    ? t("Uploaded custom sound selected", "已选择上传的自定义声音")
    : target === "spoken"
      ? t("Built-in spoken message selected", "已选择内置播报")
      : t("Built-in alarm bell selected", "已选择内置闹钟铃声");
  return true;
}

async function loadReminderSound() {
  const response = await fetch("/api/audio/reminder-sound", { cache: "no-store" });
  if (!response.ok) throw new Error("Reminder sound status unavailable");
  const status = await response.json();
  spokenSoundMode.querySelector("option[value=custom]").disabled = !status.spoken_available;
  alarmSoundMode.querySelector("option[value=custom]").disabled = !status.alarm_available;
  spokenSoundMode.value = status.spoken_mode;
  alarmSoundMode.value = status.alarm_mode;
  spokenState.textContent = status.spoken_mode === "custom"
    ? t("Uploaded custom sound selected", "已选择上传的自定义声音")
    : t("Built-in spoken message selected", "已选择内置播报");
  alarmState.textContent = status.alarm_mode === "custom"
    ? t("Uploaded custom sound selected", "已选择上传的自定义声音")
    : t("Built-in alarm bell selected", "已选择内置闹钟铃声");
}

function encodeMonoWav(samples, sampleRate) {
  const buffer = new ArrayBuffer(44 + samples.length * 2);
  const view = new DataView(buffer);
  const writeText = (offset, text) => {
    for (let index = 0; index < text.length; ++index) {
      view.setUint8(offset + index, text.charCodeAt(index));
    }
  };
  writeText(0, "RIFF");
  view.setUint32(4, buffer.byteLength - 8, true);
  writeText(8, "WAVE");
  writeText(12, "fmt ");
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, 1, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * 2, true);
  view.setUint16(32, 2, true);
  view.setUint16(34, 16, true);
  writeText(36, "data");
  view.setUint32(40, samples.length * 2, true);
  for (let index = 0; index < samples.length; ++index) {
    const sample = Math.max(-1, Math.min(1, samples[index]));
    view.setInt16(44 + index * 2,
                  sample < 0 ? sample * 32768 : sample * 32767, true);
  }
  return new Blob([buffer], { type: "audio/wav" });
}

async function convertReminderAudio(file, target) {
  const AudioContextClass = window.AudioContext || window.webkitAudioContext;
  if (!AudioContextClass || !window.OfflineAudioContext) {
    throw new Error("audio conversion unsupported");
  }
  const context = new AudioContextClass();
  try {
    const decoded = await context.decodeAudioData(await file.arrayBuffer());
    if (decoded.duration <= 0)
      throw new Error("invalid audio duration");
    /* Long songs can make mobile browsers appear frozen while rendering a
     * multi-megabyte WAV.  Fifty seconds is enough for an alarm segment and
     * the dispenser repeats it until the patient button is pressed. */
    const duration = Math.min(decoded.duration, target === "spoken" ? 30 : 50);
    const sampleRate = 16000;
    const frameCount = Math.ceil(duration * sampleRate);
    const offline = new OfflineAudioContext(1, frameCount, sampleRate);
    const source = offline.createBufferSource();
    source.buffer = decoded;
    source.connect(offline.destination);
    source.start();
    source.stop(duration);
    const rendered = await offline.startRendering();
    return encodeMonoWav(rendered.getChannelData(0), sampleRate);
  } finally {
    await context.close().catch(() => {});
  }
}

async function uploadReminderAudio(target) {
  const controls = soundControls(target);
  const file = controls.file.files[0];
  if (!file) return showToast(t("Choose an audio file first", "请先选择音频文件"));
  controls.state.textContent = t("Converting and validating...", "正在转换并验证...");
  let wav;
  try {
    wav = await convertReminderAudio(file, target);
  } catch (error) {
    const extension = file.name.includes(".")
      ? file.name.split(".").pop().toUpperCase() : "UNKNOWN";
    controls.state.textContent = t(
      `${extension} cannot be decoded by this browser`,
      `当前浏览器无法解码 ${extension}`);
    return;
  }
  const response = await fetch(`/api/audio/upload-preview?target=${target}`, {
    method: "POST", headers: { "Content-Type": "application/octet-stream" }, body: wav
  }).catch(() => null);
  if (response?.ok) {
    controls.mode.querySelector("option[value=custom]").disabled = false;
    await setReminderSound(target, "custom");
  } else {
    controls.state.textContent = t(
      "Upload rejected; previous sound kept",
      "上传失败，已保留之前的声音");
  }
}

async function playReminderPreview(target) {
  const controls = soundControls(target);
  const response = await fetch(`/api/audio/play-preview?target=${target}`, { method: "POST" }).catch(() => null);
  controls.state.textContent = response?.ok
    ? t("Playing uploaded sound", "正在播放上传的声音")
    : t("Uploaded sound unavailable or audio is busy", "上传的声音不可用或音频正忙");
}

document.querySelector("#upload-spoken").addEventListener("click", () => uploadReminderAudio("spoken"));
document.querySelector("#upload-alarm").addEventListener("click", () => uploadReminderAudio("alarm"));
document.querySelector("#play-spoken").addEventListener("click", () => playReminderPreview("spoken"));
document.querySelector("#play-alarm").addEventListener("click", () => playReminderPreview("alarm"));
document.querySelector("#save-spoken-sound").addEventListener("click", async () => {
  if (!await setReminderSound("spoken", spokenSoundMode.value))
    showToast(t("Upload a valid audio file first", "请先上传有效音频"));
});
document.querySelector("#save-alarm-sound").addEventListener("click", async () => {
  if (!await setReminderSound("alarm", alarmSoundMode.value))
    showToast(t("Upload a valid audio file first", "请先上传有效音频"));
});

loadReminderSound().catch(() => {
  spokenState.textContent = alarmState.textContent =
    t("Reminder sound status unavailable", "无法读取提醒声音状态");
});

movementConfirm.addEventListener("change", () => {
  testDispenser.disabled = !movementConfirm.checked;
});

referenceConfirm.addEventListener("change", () => {
  setTrayReference.disabled = !referenceConfirm.checked;
});

setTrayReference.addEventListener("click", async () => {
  setTrayReference.disabled = true;
  trayReferenceState.textContent = t("Saving alignment reference...", "正在保存药盘基准……");
  const response = await fetch("/api/dispenser/reference", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ confirmed: referenceConfirm.checked })
  }).catch(() => null);
  if (response?.ok) {
    const result = await response.json();
    trayReferenceState.textContent = t(
      `Start position saved at ${result.angle.toFixed(2)}°; the tray did not move.`,
      `起始位置已保存为 ${result.angle.toFixed(2)}°；药盘未转动。`);
  } else {
    trayReferenceState.textContent = t(
      "Could not save the reference. Check the angle sensor and try again.",
      "无法保存基准，请检查角度传感器后重试。");
  }
  referenceConfirm.checked = false;
});

testDispenser.addEventListener("click", async () => {
  testDispenser.disabled = true;
  dispenserTestState.textContent = t("Moving the rotary tray forward one slot...", "正在将药盘向前移动一格……");
  const response = await fetch("/api/dispenser/test", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ confirmed: movementConfirm.checked })
  }).catch(() => null);
  dispenserTestState.textContent = response?.ok
    ? t("Rotary tray moved one slot.", "药盘已向前移动一格。")
    : t("Mechanical test failed or servo is unavailable.", "机械测试失败或舵机不可用。");
  movementConfirm.checked = false;
});

fetch("/api/health", { cache: "no-store" })
  .then((response) => response.ok ? response.json() : Promise.reject())
  .then((health) => {
    document.querySelector("#connection-state").textContent = t("Device connected", "设备已连接");
    document.querySelectorAll("[data-health]").forEach((element) => {
      const ready = health[element.dataset.health] === true;
      element.className = ready ? "good" : "fault";
      element.textContent = ready ? t("Detected", "正常") : t("Unavailable", "不可用");
    });
    document.querySelector("#clock-state").textContent = health.rtc
      ? t("RTC detected; time not yet validated", "已检测到时钟模块")
      : t("RTC unavailable", "时钟模块不可用");
    if (!health.servo) {
      movementConfirm.disabled = true;
      testDispenser.disabled = true;
      dispenserTestState.textContent =
        t("Servo unavailable; automatic dispensing remains locked.", "舵机不可用；自动出药仍保持锁定。");
    }
  })
  .catch(() => { document.querySelector("#connection-state").textContent = t("Device unavailable", "设备不可用"); });

const hotspotName = document.querySelector("#hotspot-name");
const hotspotPassword = document.querySelector("#hotspot-password");
const hotspotPasswordConfirm =
  document.querySelector("#hotspot-password-confirm");
const hotspotPasswordState =
  document.querySelector("#hotspot-password-state");

fetch("/api/network/hotspot", { cache: "no-store" })
  .then((response) => response.ok ? response.json() : Promise.reject())
  .then((status) => {
    hotspotName.textContent = t(`Hotspot name: ${status.ssid}`,
                                `热点名称：${status.ssid}`);
  })
  .catch(() => {
    hotspotName.textContent = t("Hotspot information unavailable",
                                "无法读取热点信息");
  });

document.querySelector("#save-hotspot-password")
  .addEventListener("click", async () => {
    const password = hotspotPassword.value;
    if (password.length < 8 || password.length > 63) {
      hotspotPasswordState.textContent =
        t("Password must contain 8 to 63 characters.",
          "密码必须为 8 至 63 个字符。");
      return;
    }
    if (password !== hotspotPasswordConfirm.value) {
      hotspotPasswordState.textContent =
        t("The two passwords do not match.", "两次输入的密码不一致。");
      return;
    }
    const confirmed = window.confirm(t(
      "Change the setup hotspot password? The dispenser will restart and this device must reconnect.",
      "确定修改设置热点密码吗？药盒将重启，此设备需要使用新密码重新连接。"));
    if (!confirmed) return;
    const response = await fetch("/api/network/hotspot", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ password })
    }).catch(() => null);
    hotspotPasswordState.textContent = response?.ok
      ? t("Password saved. Reconnect after the dispenser restarts.",
          "密码已保存。药盒重启后请使用新密码重新连接。")
      : t("Password change failed; the existing password is unchanged.",
          "密码修改失败，原密码保持不变。");
    if (response?.ok) {
      hotspotPassword.value = "";
      hotspotPasswordConfirm.value = "";
    }
  });

const caregiverPin = document.querySelector("#caregiver-pin");
const caregiverPinState = document.querySelector("#caregiver-pin-state");
const caregiverUnlock = document.querySelector("#caregiver-unlock");
const caregiverSetPin = document.querySelector("#caregiver-set-pin");
const caregiverDisablePin = document.querySelector("#caregiver-disable-pin");

function validCaregiverPin() {
  return /^[0-9]{4,8}$/.test(caregiverPin.value);
}

async function loadCaregiverAccess() {
  const response = await fetch("/api/caregiver/access", { cache: "no-store" });
  if (!response.ok) throw new Error();
  const status = await response.json();
  caregiverPinState.textContent = !status.enabled
    ? t("PIN protection is disabled.", "PIN 保护未启用。")
    : status.unlocked
      ? t("Caregiver settings are unlocked.", "管理员设置已解锁。")
      : t("Caregiver settings are locked.", "管理员设置已锁定。");
  caregiverUnlock.disabled = !status.enabled || status.unlocked;
  caregiverDisablePin.disabled = !status.enabled || !status.unlocked;
  caregiverSetPin.textContent = status.enabled
    ? t("Change PIN", "修改 PIN")
    : t("Set PIN", "设置 PIN");
}

async function caregiverAction(action) {
  if ((action === "unlock" || action === "set") && !validCaregiverPin()) {
    caregiverPinState.textContent =
      t("Enter 4 to 8 digits.", "请输入 4–8 位数字。");
    return;
  }
  const response = await fetch("/api/caregiver/access", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ action, pin: caregiverPin.value })
  }).catch(() => null);
  if (!response?.ok) {
    caregiverPinState.textContent =
      t("PIN operation failed or the PIN is incorrect.",
        "PIN 操作失败或 PIN 不正确。");
    return;
  }
  caregiverPin.value = "";
  await loadCaregiverAccess();
}

caregiverUnlock.addEventListener("click", () => caregiverAction("unlock"));
caregiverSetPin.addEventListener("click", () => caregiverAction("set"));
caregiverDisablePin.addEventListener("click", () => {
  if (window.confirm(t("Disable caregiver PIN protection?",
                       "确定关闭管理员 PIN 保护吗？"))) {
    caregiverAction("disable");
  }
});
loadCaregiverAccess().catch(() => {
  caregiverPinState.textContent =
    t("PIN status unavailable.", "无法读取 PIN 状态。");
});

const fleetEnabled = document.querySelector("#fleet-enabled");
const fleetUrl = document.querySelector("#fleet-url");
const fleetDeviceId = document.querySelector("#fleet-device-id");
const fleetToken = document.querySelector("#fleet-token");
const fleetState = document.querySelector("#fleet-state");

fetch("/api/fleet/config", {cache:"no-store"}).then((response) => response.json()).then((config) => {
  fleetEnabled.checked = config.enabled === true;
  fleetUrl.value = config.url || "";
  fleetDeviceId.value = config.device_id || "";
  fleetState.textContent = config.token_saved
    ? t("A device token is saved. Leave the token blank to keep it.", "设备密钥已保存；留空可保持原密钥。")
    : t("Enter the one-time device token from the administrator app.", "请输入管理员 App 生成的一次性设备密钥。");
}).catch(() => { fleetState.textContent = t("Remote status unavailable.", "无法读取远程状态。"); });

document.querySelector("#save-fleet").addEventListener("click", async () => {
  if (!fleetUrl.value.startsWith("https://") || fleetDeviceId.value.length < 3) {
    fleetState.textContent = t("A valid HTTPS address and device ID are required.", "必须填写有效 HTTPS 地址和设备编号。");
    return;
  }
  const response = await fetch("/api/fleet/config", {
    method:"POST", headers:{"Content-Type":"application/json"},
    body:JSON.stringify({enabled:fleetEnabled.checked, url:fleetUrl.value,
      device_id:fleetDeviceId.value, token:fleetToken.value || null})
  }).catch(() => null);
  fleetState.textContent = response?.ok
    ? t("Pairing saved. The first heartbeat may take 30 seconds.", "配对已保存，首次上线可能需要 30 秒。")
    : t("Pairing failed. Unlock caregiver settings and check the token.", "配对失败，请解锁管理员设置并检查密钥。");
  if (response?.ok) fleetToken.value = "";
});
