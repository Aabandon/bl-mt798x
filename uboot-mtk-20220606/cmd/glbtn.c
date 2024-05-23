#include <common.h>
#include <command.h>
#include <button.h>
#include <linux/delay.h>
#include <poller.h>
#include <dm/ofnode.h>
#include <linux/stringify.h>

static struct poller_async led_p;

/* If ipaddr or serverip is empty, mtktftploop functions are affected.
Set ipaddr to CONFIG_IPADDR£¬set serverip to CONFIG_SERVERIP,
and the IP address can be used normally after the router is restarted */
static void setip(void)
{
	if ((env_get("ipaddr") == NULL) || (env_get("serverip") == NULL)) {
		env_set("ipaddr", __stringify(CONFIG_IPADDR));
		env_set("serverip", __stringify(CONFIG_SERVERIP));
		env_save();
	}
}

/* Loop TFTP download mode */
static void mtktftploop(void)
{
	env_set("autostart", "yes");
	env_set("loadaddr", __stringify(CONFIG_SYS_LOAD_ADDR));
	env_save();

	while(1) {
		run_command("tftpboot initramfs-kernel.bin", 0);
		mdelay(2000);
	}
}

void led_control(const char *cmd, const char *name, const char *arg)
{
	const char *led = ofnode_conf_read_str(name);
	char buf[128];

	if (!led)
		return;

	sprintf(buf, "%s %s %s", cmd, led, arg);

	run_command(buf, 0);
}

static void gpio_power_clr(void)
{
	ofnode node = ofnode_path("/config");
	char cmd[128];
	const u32 *val;
	int size, i;

	if (!ofnode_valid(node))
		return;

	val = ofnode_read_prop(node, "gpio_power_clr", &size);
	if (!val)
		return;

	for (i = 0; i < size / 4; i++) {
		sprintf(cmd, "gpio clear %u", fdt32_to_cpu(val[i]));
		run_command(cmd, 0);
	}
}

static void led_action_post(void *arg)
{
	led_control("ledblink", "blink_led", "0");
	led_control("led", "blink_led", "on");
}

static int do_glbtn(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *button_label = NULL;
	int ret, counter = 0;
	struct udevice *dev;
	ulong ts;

	led_control("ledblink", "blink_led", "250");
	setip();
	gpio_power_clr();

	button_label = env_get("glbtn_key");
	if (!button_label)
		button_label = "reset";

	ret = button_get_by_label(button_label, &dev);
	if (ret) {
		printf("Button '%s' not found (err=%d)\n", button_label, ret);
		return CMD_RET_FAILURE;
	}

	if (!button_get_state(dev)) {
		poller_async_register(&led_p, "led_pa");
		poller_call_async(&led_p, 1000000, led_action_post, NULL);
		return CMD_RET_SUCCESS;
	}

	led_control("ledblink", "blink_led", "500");

	printf("RESET button is pressed for: %2d second(s)", counter++);

	ts = get_timer(0);

	while (button_get_state(dev) && counter < 15) {
		if (get_timer(ts) < 1000)
			continue;

		ts = get_timer(0);

		printf("\b\b\b\b\b\b\b\b\b\b\b\b%2d second(s)", counter++);
	}

	printf("\n");

	led_control("ledblink", "blink_led", "0");

	if (counter < 10) {
		led_control("led", "system_led", "on");
		run_command("httpd", 0);
	}
	else if (counter >= 10) {
		led_control("ledblink", "system_led", "200");
		mtktftploop();
	}
	else {
		led_control("ledblink", "blink_led", "0");
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	glbtn, 1, 0, do_glbtn,
	"GL-iNet button check",
	""
);
